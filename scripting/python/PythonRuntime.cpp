#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "naru/python/PythonRuntime.h"

#include "naru/core/Log.h"
#include "naru/core/Value.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace naru::python {

namespace {

bool g_initialized = false;
// runFile() 동안만 설정되는 현재 실행 컨텍스트. 내장 모듈 함수가 이것을 통해 엔진에 접근한다.
CommandContext* g_ctx = nullptr;
// 오류 위치를 사용자 스크립트 프레임으로 맞추기 위한 현재 파일 경로.
std::string g_scriptPath;

// Python 쪽 공개 API. C로 만든 _naru 함수를 감싸 설계 문서의 ctx.variables/ctx.log 형태를 제공한다.
constexpr const char* kBootstrap = R"PY(
import sys, traceback
import _naru

class Variables:
    def get(self, name, default=None):
        return _naru.get_var(name, default)
    def set(self, name, value):
        _naru.set_var(name, value)

class Context:
    def __init__(self):
        self.variables = Variables()
    def log(self, message):
        _naru.log(str(message), 0)
    def warning(self, message):
        _naru.log(str(message), 1)

class _LogStream:
    def __init__(self, level):
        self._level = level
        self._buffer = ""
    def write(self, text):
        self._buffer += text
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            _naru.log(line, self._level)
        return len(text)
    def flush(self):
        if self._buffer:
            _naru.log(self._buffer, self._level)
            self._buffer = ""

def _install_streams():
    sys.stdout = _LogStream(0)
    sys.stderr = _LogStream(2)

def _flush_streams():
    for stream in (sys.stdout, sys.stderr):
        if isinstance(stream, _LogStream):
            stream.flush()

def _describe_error(exc, script_path):
    text = "".join(traceback.format_exception(exc))
    file, line = "", 0
    if isinstance(exc, SyntaxError):
        file, line = exc.filename or "", exc.lineno or 0
    else:
        frames = traceback.extract_tb(exc.__traceback__)
        own = [f for f in frames if f.filename == script_path]
        frame = own[-1] if own else (frames[-1] if frames else None)
        if frame is not None:
            file, line = frame.filename, frame.lineno or 0
    return (type(exc).__name__, str(exc), file, line, text)
)PY";

std::string utf8(PyObject* obj) {
    if (obj == nullptr) {
        return {};
    }
    PyObject* str = PyUnicode_Check(obj) ? Py_NewRef(obj) : PyObject_Str(obj);
    if (str == nullptr) {
        PyErr_Clear();
        return "<변환 불가>";
    }
    Py_ssize_t size = 0;
    const char* data = PyUnicode_AsUTF8AndSize(str, &size);
    std::string result = data ? std::string(data, static_cast<std::size_t>(size)) : std::string();
    if (data == nullptr) {
        PyErr_Clear();
    }
    Py_DECREF(str);
    return result;
}

PyObject* toPython(const Value& value) {
    struct Visitor {
        PyObject* operator()(std::monostate) const { Py_RETURN_NONE; }
        PyObject* operator()(bool v) const { return PyBool_FromLong(v ? 1 : 0); }
        PyObject* operator()(std::int64_t v) const { return PyLong_FromLongLong(v); }
        PyObject* operator()(double v) const { return PyFloat_FromDouble(v); }
        PyObject* operator()(const std::string& v) const {
            return PyUnicode_FromStringAndSize(v.data(), static_cast<Py_ssize_t>(v.size()));
        }
    };
    return std::visit(Visitor{}, value);
}

// 실패하면 Python 예외를 설정하고 false를 반환한다.
bool fromPython(PyObject* obj, Value& out) {
    if (obj == Py_None) {
        out = std::monostate{};
    } else if (PyBool_Check(obj)) {
        out = obj == Py_True;
    } else if (PyLong_Check(obj)) {
        long long v = PyLong_AsLongLong(obj);
        if (v == -1 && PyErr_Occurred()) {
            return false;
        }
        out = static_cast<std::int64_t>(v);
    } else if (PyFloat_Check(obj)) {
        out = PyFloat_AsDouble(obj);
    } else if (PyUnicode_Check(obj)) {
        out = utf8(obj);
    } else {
        // PyErr_Format은 ASCII 서식 문자열만 받으므로 한글 메시지는 직접 만든다.
        std::string message = "게임 변수에는 None, bool, int, float, str만 저장할 수 있습니다 (받은 형식: ";
        message += Py_TYPE(obj)->tp_name;
        message += ")";
        PyErr_SetString(PyExc_TypeError, message.c_str());
        return false;
    }
    return true;
}

bool requireContext() {
    if (g_ctx == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "실행 중인 Naru 세션이 없습니다");
        return false;
    }
    return true;
}

PyObject* naruLog(PyObject*, PyObject* args) {
    const char* message = nullptr;
    int level = 0;
    if (!PyArg_ParseTuple(args, "si", &message, &level) || !requireContext()) {
        return nullptr;
    }
    LogEntry entry;
    entry.level = level == 2 ? LogLevel::Error : level == 1 ? LogLevel::Warning : LogLevel::Info;
    entry.message = message;
    entry.source.file = g_scriptPath;
    // 호출한 스크립트 행을 기록한다.
    if (PyFrameObject* frame = PyEval_GetFrame()) {
        Py_INCREF(frame);
        PyFrameObject* current = frame;
        // 내부 bootstrap 프레임을 건너뛰고 사용자 파일의 프레임을 찾는다.
        while (current != nullptr) {
            PyCodeObject* code = PyFrame_GetCode(current);
            PyObject* filename = PyObject_GetAttrString(reinterpret_cast<PyObject*>(code), "co_filename");
            std::string file = utf8(filename);
            Py_XDECREF(filename);
            Py_DECREF(code);
            if (file == g_scriptPath) {
                entry.source.line = PyFrame_GetLineNumber(current);
                break;
            }
            PyFrameObject* back = PyFrame_GetBack(current);
            Py_DECREF(current);
            current = back;
        }
        Py_XDECREF(current);
    }
    g_ctx->logger.log(std::move(entry));
    Py_RETURN_NONE;
}

PyObject* naruGetVar(PyObject*, PyObject* args) {
    const char* name = nullptr;
    PyObject* fallback = Py_None;
    if (!PyArg_ParseTuple(args, "s|O", &name, &fallback) || !requireContext()) {
        return nullptr;
    }
    auto value = g_ctx->variables.get(name);
    if (!value) {
        return Py_NewRef(fallback);
    }
    return toPython(*value);
}

PyObject* naruSetVar(PyObject*, PyObject* args) {
    const char* name = nullptr;
    PyObject* obj = nullptr;
    if (!PyArg_ParseTuple(args, "sO", &name, &obj) || !requireContext()) {
        return nullptr;
    }
    Value value;
    if (!fromPython(obj, value)) {
        return nullptr;
    }
    g_ctx->variables.set(name, std::move(value));
    Py_RETURN_NONE;
}

PyMethodDef kMethods[] = {
    {"log", naruLog, METH_VARARGS, "log(message, level)"},
    {"get_var", naruGetVar, METH_VARARGS, "get_var(name, default=None)"},
    {"set_var", naruSetVar, METH_VARARGS, "set_var(name, value)"},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kModule = {PyModuleDef_HEAD_INIT, "_naru", "Naru 엔진 내장 연결", -1, kMethods};

PyObject* initNaruModule() {
    return PyModule_Create(&kModule);
}

// 현재 설정된 Python 예외를 ScriptError로 바꾸고 지운다.
ScriptError takeError() {
    ScriptError error;
    PyObject* exc = PyErr_GetRaisedException();
    if (exc == nullptr) {
        error.type = "UnknownError";
        error.message = "알 수 없는 오류";
        return error;
    }
    error.type = Py_TYPE(exc)->tp_name;
    error.message = utf8(exc);

    PyObject* naru = PyImport_ImportModule("naru");
    PyObject* described =
        naru ? PyObject_CallMethod(naru, "_describe_error", "Os", exc, g_scriptPath.c_str()) : nullptr;
    if (described != nullptr && PyTuple_Check(described) && PyTuple_Size(described) == 5) {
        error.type = utf8(PyTuple_GetItem(described, 0));
        error.message = utf8(PyTuple_GetItem(described, 1));
        error.file = utf8(PyTuple_GetItem(described, 2));
        error.line = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(described, 3)));
        error.traceback = utf8(PyTuple_GetItem(described, 4));
    } else {
        PyErr_Clear();
        error.file = g_scriptPath;
    }
    Py_XDECREF(described);
    Py_XDECREF(naru);
    Py_DECREF(exc);
    return error;
}

ScriptResult failure(ScriptError error) {
    ScriptResult result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

ScriptResult failure(std::string type, std::string message) {
    ScriptError error;
    error.type = std::move(type);
    error.message = std::move(message);
    error.file = g_scriptPath;
    return failure(std::move(error));
}

bool readSource(const std::string& path, std::string& out) {
    std::ifstream file(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())),
                       std::ios::binary);
    if (!file) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    // UTF-8 BOM 제거
    if (out.size() >= 3 && out.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        out.erase(0, 3);
    }
    return true;
}

// 소스를 읽고 컴파일한다. 실패하면 nullptr와 함께 result에 오류를 채운다.
PyObject* compileFile(const std::string& path, ScriptResult& result) {
    std::string source;
    if (!readSource(path, source)) {
        result = failure("FileNotFoundError", "스크립트 파일을 열 수 없습니다: " + path);
        return nullptr;
    }
    PyObject* code = Py_CompileString(source.c_str(), path.c_str(), Py_file_input);
    if (code == nullptr) {
        result = failure(takeError());
    }
    return code;
}

struct ScriptScope {
    ScriptScope(const std::string& path, CommandContext* ctx) {
        g_scriptPath = path;
        g_ctx = ctx;
    }
    ~ScriptScope() {
        g_ctx = nullptr;
        g_scriptPath.clear();
    }
};

std::filesystem::path resolveHome(const std::string& requested) {
    if (!requested.empty()) {
        return std::filesystem::path(reinterpret_cast<const char8_t*>(requested.c_str()));
    }
#ifdef _WIN32
    if (const wchar_t* env = _wgetenv(L"NARU_PYTHON_HOME"); env != nullptr && *env != L'\0') {
        return std::filesystem::path(env);
    }
#else
    if (const char* env = std::getenv("NARU_PYTHON_HOME"); env != nullptr && *env != '\0') {
        return std::filesystem::path(env);
    }
#endif
#ifdef NARU_PYTHON_DEFAULT_HOME
    return std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_PYTHON_DEFAULT_HOME));
#else
    return {};
#endif
}

void checkStatus(PyStatus status, PyConfig& config) {
    if (PyStatus_Exception(status)) {
        std::string message = status.err_msg ? status.err_msg : "알 수 없는 오류";
        PyConfig_Clear(&config);
        throw std::runtime_error("Python 초기화 실패: " + message);
    }
}

} // namespace

std::string ScriptResult::summary() const {
    if (ok) {
        return "성공";
    }
    std::string location = error.file;
    if (error.line > 0) {
        location += ":" + std::to_string(error.line);
    }
    return (location.empty() ? std::string() : location + ": ") + error.type + ": " + error.message;
}

PythonRuntime::PythonRuntime(const std::string& pythonHome) {
    if (g_initialized) {
        throw std::runtime_error("PythonRuntime은 프로세스당 하나만 만들 수 있습니다");
    }
    if (PyImport_AppendInittab("_naru", initNaruModule) == -1) {
        throw std::runtime_error("_naru 모듈 등록 실패");
    }

    PyConfig config;
    // 격리 모드: 사용자 환경변수(PYTHONPATH 등)와 user site-packages의 영향을 받지 않는다.
    PyConfig_InitIsolatedConfig(&config);
    config.install_signal_handlers = 0;
    std::filesystem::path home = resolveHome(pythonHome);
    if (!home.empty()) {
        checkStatus(PyConfig_SetString(&config, &config.home, home.wstring().c_str()), config);
    }
    checkStatus(Py_InitializeFromConfig(&config), config);
    PyConfig_Clear(&config);
    g_initialized = true;

    PyObject* module = PyImport_AddModuleRef("naru");
    PyObject* globals = module ? PyModule_GetDict(module) : nullptr; // borrowed
    PyObject* result = nullptr;
    if (globals != nullptr) {
        PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
        result = PyRun_String(kBootstrap, Py_file_input, globals, globals);
    }
    if (result == nullptr) {
        PyErr_Print();
        Py_XDECREF(module);
        Py_Finalize();
        g_initialized = false;
        throw std::runtime_error("naru Python 모듈 초기화 실패");
    }
    Py_DECREF(result);
    PyObject* installed = PyObject_CallMethod(module, "_install_streams", nullptr);
    Py_XDECREF(installed);
    Py_DECREF(module);
    PyErr_Clear();
}

PythonRuntime::~PythonRuntime() {
    if (g_initialized) {
        Py_Finalize();
        g_initialized = false;
    }
}

std::string PythonRuntime::version() const {
    return Py_GetVersion();
}

ScriptResult PythonRuntime::checkSyntax(const std::string& path) {
    ScriptScope scope(path, nullptr);
    ScriptResult result;
    PyObject* code = compileFile(path, result);
    Py_XDECREF(code);
    return result;
}

ScriptResult PythonRuntime::runFile(const std::string& path, CommandContext& ctx,
                                    const std::string& functionName) {
    ScriptScope scope(path, &ctx);
    ScriptResult result;

    PyObject* code = compileFile(path, result);
    if (code == nullptr) {
        return result;
    }

    // 매 실행마다 새 전역 공간을 사용해 이전 실행의 상태가 남지 않게 한다.
    PyObject* globals = PyDict_New();
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
    PyObject* name = PyUnicode_FromString("__naru_script__");
    PyObject* file = PyUnicode_FromString(path.c_str());
    PyDict_SetItemString(globals, "__name__", name);
    PyDict_SetItemString(globals, "__file__", file);
    Py_DECREF(name);
    Py_DECREF(file);

    PyObject* naru = PyImport_ImportModule("naru");
    PyObject* executed = PyEval_EvalCode(code, globals, globals);
    Py_DECREF(code);

    if (executed == nullptr) {
        result = failure(takeError());
    } else {
        Py_DECREF(executed);
        PyObject* function = PyDict_GetItemString(globals, functionName.c_str()); // borrowed
        if (function == nullptr || !PyCallable_Check(function)) {
            result = failure("NameError", "스크립트에 호출할 함수 '" + functionName + "'가 없습니다");
        } else {
            PyObject* pyCtx = naru ? PyObject_CallMethod(naru, "Context", nullptr) : nullptr;
            PyObject* returned = pyCtx ? PyObject_CallOneArg(function, pyCtx) : nullptr;
            if (returned == nullptr) {
                result = failure(takeError());
            } else if (PyCoro_CheckExact(returned)) {
                PyObject* closed = PyObject_CallMethod(returned, "close", nullptr);
                Py_XDECREF(closed);
                PyErr_Clear();
                result = failure("NotImplementedError",
                                 "async 함수는 아직 지원하지 않습니다 (대기형 명령은 M2에서 구현 예정)");
            }
            Py_XDECREF(returned);
            Py_XDECREF(pyCtx);
        }
    }

    if (naru != nullptr) {
        PyObject* flushed = PyObject_CallMethod(naru, "_flush_streams", nullptr);
        Py_XDECREF(flushed);
        Py_DECREF(naru);
    }
    PyErr_Clear();
    Py_DECREF(globals);

    if (!result.ok) {
        LogEntry entry;
        entry.level = LogLevel::Error;
        entry.message = result.error.type + ": " + result.error.message;
        entry.source = {result.error.file, result.error.line, ""};
        entry.detail = result.error.traceback;
        ctx.logger.log(std::move(entry));
    }
    return result;
}

} // namespace naru::python
