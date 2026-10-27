#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "naru/python/PythonRuntime.h"

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/data/Database.h"
#include "naru/runtime/GameWorld.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>

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

class Database:
    """프로젝트 데이터베이스 (읽기 전용).
    get("monster", "slime") -> {"id": "slime", "name": "슬라임", "hp": 30, ...} 또는 None"""
    def get(self, type_key, record_id):
        return _naru.db_get(type_key, record_id)
    def ids(self, type_key):
        return _naru.db_ids(type_key)
    def types(self):
        return _naru.db_types()

class Input:
    """입력 동작 (게임 실행 중).
    held("left") 누르고 있는가, pressed("confirm") 이번 프레임에 새로 눌렀는가,
    bind("dash", ["Shift"]) 키 배치 바꾸기/새 동작 만들기, keys("confirm"), actions()"""
    def held(self, action):
        return _naru.input_held(action)
    def pressed(self, action):
        return _naru.input_pressed(action)
    def bind(self, action, keys):
        if isinstance(keys, str):
            keys = [keys]
        _naru.input_bind(action, [str(k) for k in keys])
    def keys(self, action):
        return _naru.input_keys(action)
    def actions(self):
        return _naru.input_actions()

def _rw(name):
    return property(lambda self: _naru.player_get(name), lambda self, v: _naru.player_set(name, v))

def _ro(getter, name):
    return property(lambda self: getter(name))

class Player:
    """현재 장면의 플레이어 객체 (게임 실행 중).
    x, y, speed(픽셀/초), facing("down"/"up"/"left"/"right"), default_control(기본 조작 켜기/끄기)은 바꿀 수 있다.
    move(dx, dy): 충돌을 따져 이동, interact(): 앞의 객체 조사"""
    id = _ro(_naru.player_get, "id")
    name = _ro(_naru.player_get, "name")
    width = _ro(_naru.player_get, "width")
    height = _ro(_naru.player_get, "height")
    x = _rw("x")
    y = _rw("y")
    speed = _rw("speed")
    facing = _rw("facing")
    default_control = _rw("default_control")
    def move(self, dx, dy):
        return _naru.player_move(float(dx), float(dy))
    def interact(self):
        return _naru.player_interact()

class SceneObject:
    """장면의 객체 하나 (게임 실행 중). ctx.scene.object("obj_002") / find("상자") / objects()로 얻는다.
    x, y, passable(통과 가능), visible(보이기), color("#rrggbb")는 바꿀 수 있다. move(dx, dy): 충돌을 따져 이동."""
    def __init__(self, object_id):
        self._id = object_id
    def __repr__(self):
        return f"<SceneObject {self._id}>"
    def __eq__(self, other):
        return isinstance(other, SceneObject) and other._id == self._id
    def __hash__(self):
        return hash(self._id)
    id = property(lambda self: self._id)
    name = property(lambda self: _naru.object_get(self._id, "name"))
    width = property(lambda self: _naru.object_get(self._id, "width"))
    height = property(lambda self: _naru.object_get(self._id, "height"))
    is_player = property(lambda self: _naru.object_get(self._id, "is_player"))
    x = property(lambda self: _naru.object_get(self._id, "x"), lambda self, v: _naru.object_set(self._id, "x", v))
    y = property(lambda self: _naru.object_get(self._id, "y"), lambda self, v: _naru.object_set(self._id, "y", v))
    passable = property(lambda self: _naru.object_get(self._id, "passable"),
                        lambda self, v: _naru.object_set(self._id, "passable", v))
    visible = property(lambda self: _naru.object_get(self._id, "visible"),
                       lambda self, v: _naru.object_set(self._id, "visible", v))
    color = property(lambda self: _naru.object_get(self._id, "color"),
                     lambda self, v: _naru.object_set(self._id, "color", v))
    def move(self, dx, dy):
        return _naru.object_move(self._id, float(dx), float(dy))

class Scene:
    """현재 장면 (게임 실행 중). change("scene_002")로 장면 이동 (이번 틱이 끝난 뒤 바뀐다).
    object(ID) / find(이름) → SceneObject 또는 None, objects() → 모든 객체"""
    id = _ro(_naru.scene_get, "id")
    name = _ro(_naru.scene_get, "name")
    width = _ro(_naru.scene_get, "width")
    height = _ro(_naru.scene_get, "height")
    def change(self, scene_id):
        _naru.scene_change(scene_id)
    def ids(self):
        return _naru.scene_ids()
    def object(self, object_id):
        return SceneObject(object_id) if _naru.object_exists(object_id) else None
    def find(self, name):
        found = _naru.scene_find(name)
        return SceneObject(found) if found is not None else None
    def objects(self):
        return [SceneObject(i) for i in _naru.scene_object_ids()]
class Context:
    def __init__(self):
        self.variables = Variables()
        self.db = Database()
        game = _naru.has_game()
        # 게임 밖(에디터 테스트 실행, 단독 스크립트 실행)에서는 None
        self.input = Input() if game else None
        self.scene = Scene() if game and _naru.has_scene() else None
        self.player = Player() if game and _naru.has_player() else None
    @property
    def busy(self):
        """대사나 플레이어를 멈추는 이벤트가 진행 중인가 (게임 밖이면 False)"""
        return _naru.game_busy()
    def log(self, message):
        _naru.log(str(message), 0)
    def warning(self, message):
        _naru.log(str(message), 1)

def _global_names(path, source):
    """전역 스크립트의 파일 최상위 변수 이름과 행 (import·함수·_로 시작하는 이름 제외)."""
    import ast
    tree = ast.parse(source, filename=path)
    names = {}
    for node in tree.body:
        if isinstance(node, ast.Assign):
            targets = node.targets
        elif isinstance(node, (ast.AnnAssign, ast.AugAssign)):
            targets = [node.target]
        else:
            continue
        for target in targets:
            for n in ast.walk(target):
                if isinstance(n, ast.Name) and not n.id.startswith("_"):
                    names.setdefault(n.id, node.lineno)
    return list(names.items())

def _is_standard_module(name):
    return name in sys.builtin_module_names or name in getattr(sys, "stdlib_module_names", ())

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

const naru::Database* requireDatabase() {
    if (!requireContext()) {
        return nullptr;
    }
    if (g_ctx->database == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "데이터베이스가 연결되지 않았습니다");
        return nullptr;
    }
    return g_ctx->database;
}

PyObject* stringObject(const std::string& s) {
    return PyUnicode_FromStringAndSize(s.data(), static_cast<Py_ssize_t>(s.size()));
}

// dict에 값을 넣는다 (참조는 dict가 가져간다). 실패하면 false.
bool setItem(PyObject* dict, const std::string& key, PyObject* value) {
    if (value == nullptr) {
        return false;
    }
    int rc = PyDict_SetItemString(dict, key.c_str(), value);
    Py_DECREF(value);
    return rc == 0;
}

PyObject* naruDbGet(PyObject*, PyObject* args) {
    const char* typeKey = nullptr;
    const char* recordId = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &typeKey, &recordId)) {
        return nullptr;
    }
    const naru::Database* db = requireDatabase();
    if (!db) {
        return nullptr;
    }
    const naru::DataType* type = db->findType(typeKey);
    if (!type) {
        std::string message = std::string("데이터 종류가 없습니다: ") + typeKey;
        PyErr_SetString(PyExc_KeyError, message.c_str());
        return nullptr;
    }
    const naru::DataRecord* record = type->findRecord(recordId);
    if (!record) {
        Py_RETURN_NONE;
    }
    PyObject* dict = PyDict_New();
    bool ok = dict && setItem(dict, "id", stringObject(record->id)) && setItem(dict, "name", stringObject(record->name));
    for (const auto& field : type->fields) {
        ok = ok && setItem(dict, field.key, toPython(type->valueOf(*record, field)));
    }
    if (!ok) {
        Py_XDECREF(dict);
        return nullptr;
    }
    return dict;
}

PyObject* naruDbIds(PyObject*, PyObject* args) {
    const char* typeKey = nullptr;
    if (!PyArg_ParseTuple(args, "s", &typeKey)) {
        return nullptr;
    }
    const naru::Database* db = requireDatabase();
    if (!db) {
        return nullptr;
    }
    const naru::DataType* type = db->findType(typeKey);
    if (!type) {
        std::string message = std::string("데이터 종류가 없습니다: ") + typeKey;
        PyErr_SetString(PyExc_KeyError, message.c_str());
        return nullptr;
    }
    PyObject* list = PyList_New(0);
    for (const auto& record : type->records) {
        PyObject* id = stringObject(record.id);
        if (!list || !id || PyList_Append(list, id) != 0) {
            Py_XDECREF(id);
            Py_XDECREF(list);
            return nullptr;
        }
        Py_DECREF(id);
    }
    return list;
}

PyObject* naruDbTypes(PyObject*, PyObject*) {
    const naru::Database* db = requireDatabase();
    if (!db) {
        return nullptr;
    }
    PyObject* list = PyList_New(0);
    for (const auto& type : db->types) {
        PyObject* key = stringObject(type.key);
        if (!list || !key || PyList_Append(list, key) != 0) {
            Py_XDECREF(key);
            Py_XDECREF(list);
            return nullptr;
        }
        Py_DECREF(key);
    }
    return list;
}

// ---- 게임 실행 상태 (ctx.input / ctx.player / ctx.scene)

GameWorld* requireGame() {
    if (!requireContext()) {
        return nullptr;
    }
    if (g_ctx->game == nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "게임 실행 중에만 쓸 수 있습니다 (에디터 테스트 실행에서는 ctx.player 등이 None)");
        return nullptr;
    }
    return g_ctx->game;
}

const SceneObject* requirePlayer(GameWorld* game) {
    const SceneObject* player = game ? game->player() : nullptr;
    if (game && !player) {
        PyErr_SetString(PyExc_RuntimeError, "이 장면에는 플레이어 객체가 없습니다");
    }
    return player;
}

PyObject* stringList(const std::vector<std::string>& items) {
    PyObject* list = PyList_New(0);
    for (const std::string& s : items) {
        PyObject* item = stringObject(s);
        if (!list || !item || PyList_Append(list, item) != 0) {
            Py_XDECREF(item);
            Py_XDECREF(list);
            return nullptr;
        }
        Py_DECREF(item);
    }
    return list;
}

PyObject* naruHasGame(PyObject*, PyObject*) {
    return PyBool_FromLong(g_ctx && g_ctx->game ? 1 : 0);
}

PyObject* naruHasScene(PyObject*, PyObject*) {
    return PyBool_FromLong(g_ctx && g_ctx->game && g_ctx->game->scene() ? 1 : 0);
}

PyObject* naruHasPlayer(PyObject*, PyObject*) {
    return PyBool_FromLong(g_ctx && g_ctx->game && g_ctx->game->player() ? 1 : 0);
}

PyObject* naruGameBusy(PyObject*, PyObject*) {
    return PyBool_FromLong(g_ctx && g_ctx->game && g_ctx->game->busy() ? 1 : 0);
}

PyObject* naruInputHeld(PyObject*, PyObject* args) {
    const char* action = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &action) || !(game = requireGame())) {
        return nullptr;
    }
    return PyBool_FromLong(game->input().isHeld(action) ? 1 : 0);
}

PyObject* naruInputPressed(PyObject*, PyObject* args) {
    const char* action = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &action) || !(game = requireGame())) {
        return nullptr;
    }
    return PyBool_FromLong(game->input().wasPressed(action) ? 1 : 0);
}

PyObject* naruInputBind(PyObject*, PyObject* args) {
    const char* action = nullptr;
    PyObject* list = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "sO!", &action, &PyList_Type, &list) || !(game = requireGame())) {
        return nullptr;
    }
    if (!InputMap::isValidActionName(action)) {
        std::string message = std::string("동작 이름은 영문 소문자로 시작하고 소문자·숫자·_만 쓸 수 있습니다: ") + action;
        PyErr_SetString(PyExc_ValueError, message.c_str());
        return nullptr;
    }
    std::vector<std::string> keys;
    for (Py_ssize_t i = 0; i < PyList_Size(list); ++i) {
        keys.push_back(utf8(PyList_GetItem(list, i)));
    }
    game->inputMap().bind(action, std::move(keys));
    Py_RETURN_NONE;
}

PyObject* naruInputKeys(PyObject*, PyObject* args) {
    const char* action = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &action) || !(game = requireGame())) {
        return nullptr;
    }
    return stringList(game->inputMap().keys(action));
}

PyObject* naruInputActions(PyObject*, PyObject*) {
    GameWorld* game = requireGame();
    return game ? stringList(game->inputMap().actions()) : nullptr;
}

PyObject* naruPlayerGet(PyObject*, PyObject* args) {
    const char* name = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &name) || !(game = requireGame())) {
        return nullptr;
    }
    const SceneObject* p = requirePlayer(game);
    if (!p) {
        return nullptr;
    }
    std::string key = name;
    if (key == "id") return stringObject(p->id);
    if (key == "name") return stringObject(p->name);
    if (key == "x") return PyFloat_FromDouble(p->x);
    if (key == "y") return PyFloat_FromDouble(p->y);
    if (key == "width") return PyFloat_FromDouble(p->width);
    if (key == "height") return PyFloat_FromDouble(p->height);
    if (key == "speed") return PyFloat_FromDouble(game->playerSpeed());
    if (key == "facing") return stringObject(toKey(game->facing()));
    if (key == "default_control") return PyBool_FromLong(game->defaultControl() ? 1 : 0);
    PyErr_SetString(PyExc_AttributeError, name);
    return nullptr;
}

PyObject* naruPlayerSet(PyObject*, PyObject* args) {
    const char* name = nullptr;
    PyObject* value = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "sO", &name, &value) || !(game = requireGame())) {
        return nullptr;
    }
    const SceneObject* p = requirePlayer(game);
    if (!p) {
        return nullptr;
    }
    std::string key = name;
    if (key == "facing") {
        auto facing = facingFromKey(utf8(value));
        if (!facing) {
            PyErr_SetString(PyExc_ValueError, "facing은 \"down\", \"up\", \"left\", \"right\" 중 하나입니다");
            return nullptr;
        }
        game->setFacing(*facing);
        Py_RETURN_NONE;
    }
    if (key == "default_control") {
        game->setDefaultControl(PyObject_IsTrue(value) == 1);
        Py_RETURN_NONE;
    }
    double number = PyFloat_AsDouble(value);
    if (number == -1.0 && PyErr_Occurred()) {
        PyErr_Clear();
        std::string message = "'" + key + "'에는 숫자를 넣어야 합니다";
        PyErr_SetString(PyExc_TypeError, message.c_str());
        return nullptr;
    }
    if (key == "x") {
        game->setPlayerPosition(number, p->y);
    } else if (key == "y") {
        game->setPlayerPosition(p->x, number);
    } else if (key == "speed") {
        if (number < 0) {
            PyErr_SetString(PyExc_ValueError, "speed는 0 이상이어야 합니다");
            return nullptr;
        }
        game->setPlayerSpeed(number);
    } else {
        std::string message = "바꿀 수 없는 속성입니다: " + key;
        PyErr_SetString(PyExc_AttributeError, message.c_str());
        return nullptr;
    }
    Py_RETURN_NONE;
}

PyObject* naruPlayerMove(PyObject*, PyObject* args) {
    double dx = 0;
    double dy = 0;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "dd", &dx, &dy) || !(game = requireGame()) || !requirePlayer(game)) {
        return nullptr;
    }
    return PyBool_FromLong(game->movePlayer(dx, dy) ? 1 : 0);
}

PyObject* naruPlayerInteract(PyObject*, PyObject*) {
    GameWorld* game = requireGame();
    if (!game || !requirePlayer(game)) {
        return nullptr;
    }
    return PyBool_FromLong(game->interact() ? 1 : 0);
}

PyObject* naruSceneGet(PyObject*, PyObject* args) {
    const char* name = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &name) || !(game = requireGame())) {
        return nullptr;
    }
    const Scene* scene = game->scene();
    if (!scene) {
        PyErr_SetString(PyExc_RuntimeError, "아직 장면에 들어가지 않았습니다");
        return nullptr;
    }
    std::string key = name;
    if (key == "id") return stringObject(scene->id);
    if (key == "name") return stringObject(scene->name);
    if (key == "width") return PyLong_FromLong(scene->width);
    if (key == "height") return PyLong_FromLong(scene->height);
    PyErr_SetString(PyExc_AttributeError, name);
    return nullptr;
}

PyObject* naruSceneChange(PyObject*, PyObject* args) {
    const char* sceneId = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &sceneId) || !(game = requireGame())) {
        return nullptr;
    }
    if (!game->project().findScene(sceneId)) {
        std::string message = std::string("장면이 없습니다: ") + sceneId;
        PyErr_SetString(PyExc_KeyError, message.c_str());
        return nullptr;
    }
    game->requestScene(sceneId);
    Py_RETURN_NONE;
}

PyObject* naruSceneIds(PyObject*, PyObject*) {
    GameWorld* game = requireGame();
    if (!game) {
        return nullptr;
    }
    std::vector<std::string> ids;
    for (const Scene& s : game->project().scenes) {
        ids.push_back(s.id);
    }
    return stringList(ids);
}

// ---- 장면 객체 (ctx.scene.object)

const SceneObject* requireSceneObject(GameWorld* game, const char* id) {
    const SceneObject* o = game ? game->object(id) : nullptr;
    if (game && !o) {
        std::string message = std::string("장면에 없는 객체입니다: ") + id + " (장면이 바뀌었거나 ID가 틀림)";
        PyErr_SetString(PyExc_KeyError, message.c_str());
    }
    return o;
}

PyObject* naruObjectExists(PyObject*, PyObject* args) {
    const char* id = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &id) || !(game = requireGame())) {
        return nullptr;
    }
    return PyBool_FromLong(game->object(id) ? 1 : 0);
}

PyObject* naruObjectGet(PyObject*, PyObject* args) {
    const char* id = nullptr;
    const char* name = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "ss", &id, &name) || !(game = requireGame())) {
        return nullptr;
    }
    const SceneObject* o = requireSceneObject(game, id);
    if (!o) {
        return nullptr;
    }
    std::string key = name;
    if (key == "name") return stringObject(o->name);
    if (key == "x") return PyFloat_FromDouble(o->x);
    if (key == "y") return PyFloat_FromDouble(o->y);
    if (key == "width") return PyFloat_FromDouble(o->width);
    if (key == "height") return PyFloat_FromDouble(o->height);
    if (key == "passable") return PyBool_FromLong(o->passable ? 1 : 0);
    if (key == "visible") return PyBool_FromLong(o->visible ? 1 : 0);
    if (key == "color") return stringObject(o->color);
    if (key == "is_player") return PyBool_FromLong(game->player() == o ? 1 : 0);
    PyErr_SetString(PyExc_AttributeError, name);
    return nullptr;
}

PyObject* naruObjectSet(PyObject*, PyObject* args) {
    const char* id = nullptr;
    const char* name = nullptr;
    PyObject* value = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "ssO", &id, &name, &value) || !(game = requireGame())) {
        return nullptr;
    }
    const SceneObject* o = requireSceneObject(game, id);
    if (!o) {
        return nullptr;
    }
    std::string key = name;
    if (key == "passable") {
        game->setObjectPassable(id, PyObject_IsTrue(value) == 1);
        Py_RETURN_NONE;
    }
    if (key == "visible") {
        game->setObjectVisible(id, PyObject_IsTrue(value) == 1);
        Py_RETURN_NONE;
    }
    if (key == "color") {
        std::string color = utf8(value);
        bool ok = color.size() == 7 && color[0] == '#' &&
                  color.find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos;
        if (!ok) {
            PyErr_SetString(PyExc_ValueError, "color는 \"#rrggbb\" 형식이어야 합니다 (예: \"#ff8800\")");
            return nullptr;
        }
        game->setObjectColor(id, color);
        Py_RETURN_NONE;
    }
    double number = PyFloat_AsDouble(value);
    if (number == -1.0 && PyErr_Occurred()) {
        PyErr_Clear();
        std::string message = "'" + key + "'에는 숫자를 넣어야 합니다";
        PyErr_SetString(PyExc_TypeError, message.c_str());
        return nullptr;
    }
    if (key == "x") {
        game->setObjectPosition(id, number, o->y);
    } else if (key == "y") {
        game->setObjectPosition(id, o->x, number);
    } else {
        std::string message = "바꿀 수 없는 속성입니다: " + key;
        PyErr_SetString(PyExc_AttributeError, message.c_str());
        return nullptr;
    }
    Py_RETURN_NONE;
}

PyObject* naruObjectMove(PyObject*, PyObject* args) {
    const char* id = nullptr;
    double dx = 0;
    double dy = 0;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "sdd", &id, &dx, &dy) || !(game = requireGame()) || !requireSceneObject(game, id)) {
        return nullptr;
    }
    return PyBool_FromLong(game->moveObject(id, dx, dy) ? 1 : 0);
}

PyObject* naruSceneFind(PyObject*, PyObject* args) {
    const char* name = nullptr;
    GameWorld* game = nullptr;
    if (!PyArg_ParseTuple(args, "s", &name) || !(game = requireGame())) {
        return nullptr;
    }
    if (const Scene* scene = game->scene()) {
        for (const SceneObject& o : scene->objects) {
            if (o.name == name) {
                return stringObject(o.id);
            }
        }
    }
    Py_RETURN_NONE;
}

PyObject* naruSceneObjectIds(PyObject*, PyObject*) {
    GameWorld* game = requireGame();
    if (!game) {
        return nullptr;
    }
    std::vector<std::string> ids;
    if (const Scene* scene = game->scene()) {
        for (const SceneObject& o : scene->objects) {
            ids.push_back(o.id);
        }
    }
    return stringList(ids);
}

PyMethodDef kMethods[] = {
    {"log", naruLog, METH_VARARGS, "log(message, level)"},
    {"get_var", naruGetVar, METH_VARARGS, "get_var(name, default=None)"},
    {"set_var", naruSetVar, METH_VARARGS, "set_var(name, value)"},
    {"db_get", naruDbGet, METH_VARARGS, "db_get(type_key, record_id)"},
    {"db_ids", naruDbIds, METH_VARARGS, "db_ids(type_key)"},
    {"db_types", naruDbTypes, METH_NOARGS, "db_types()"},
    {"has_game", naruHasGame, METH_NOARGS, "has_game()"},
    {"has_scene", naruHasScene, METH_NOARGS, "has_scene()"},
    {"has_player", naruHasPlayer, METH_NOARGS, "has_player()"},
    {"game_busy", naruGameBusy, METH_NOARGS, "game_busy()"},
    {"input_held", naruInputHeld, METH_VARARGS, "input_held(action)"},
    {"input_pressed", naruInputPressed, METH_VARARGS, "input_pressed(action)"},
    {"input_bind", naruInputBind, METH_VARARGS, "input_bind(action, keys)"},
    {"input_keys", naruInputKeys, METH_VARARGS, "input_keys(action)"},
    {"input_actions", naruInputActions, METH_NOARGS, "input_actions()"},
    {"player_get", naruPlayerGet, METH_VARARGS, "player_get(name)"},
    {"player_set", naruPlayerSet, METH_VARARGS, "player_set(name, value)"},
    {"player_move", naruPlayerMove, METH_VARARGS, "player_move(dx, dy)"},
    {"player_interact", naruPlayerInteract, METH_NOARGS, "player_interact()"},
    {"scene_get", naruSceneGet, METH_VARARGS, "scene_get(name)"},
    {"scene_change", naruSceneChange, METH_VARARGS, "scene_change(scene_id)"},
    {"scene_ids", naruSceneIds, METH_NOARGS, "scene_ids()"},
    {"scene_find", naruSceneFind, METH_VARARGS, "scene_find(name)"},
    {"scene_object_ids", naruSceneObjectIds, METH_NOARGS, "scene_object_ids()"},
    {"object_exists", naruObjectExists, METH_VARARGS, "object_exists(id)"},
    {"object_get", naruObjectGet, METH_VARARGS, "object_get(id, name)"},
    {"object_set", naruObjectSet, METH_VARARGS, "object_set(id, name, value)"},
    {"object_move", naruObjectMove, METH_VARARGS, "object_move(id, dx, dy)"},
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
        unloadModules();
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

namespace {

// 전역 스크립트 모듈 (경로 → 모듈 객체). Python을 끝내기 전에 unloadModules()로 정리한다.
std::map<std::string, PyObject*> g_modules;

// 경로 문자열은 UTF-8이다. string()은 Windows 코드 페이지로 바꾸므로 쓰지 않는다.
std::string u8(const std::filesystem::path& p) {
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::string moduleNameOf(const std::string& path) {
    return u8(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())).stem());
}

void logFailure(const ScriptResult& result, CommandContext& ctx) {
    if (result.ok) {
        return;
    }
    LogEntry entry;
    entry.level = LogLevel::Error;
    entry.message = result.error.type + ": " + result.error.message;
    entry.source = {result.error.file, result.error.line, ""};
    entry.detail = result.error.traceback;
    ctx.logger.log(std::move(entry));
}

void flushStreams() {
    if (PyObject* naru = PyImport_ImportModule("naru")) {
        PyObject* flushed = PyObject_CallMethod(naru, "_flush_streams", nullptr);
        Py_XDECREF(flushed);
        Py_DECREF(naru);
    }
    PyErr_Clear();
}

// function(ctx)를 부른다. returned가 있으면 반환값을 변환해 넣는다.
ScriptResult invoke(PyObject* function, Value* returned) {
    ScriptResult result;
    PyObject* naru = PyImport_ImportModule("naru");
    PyObject* pyCtx = naru ? PyObject_CallMethod(naru, "Context", nullptr) : nullptr;
    PyObject* value = pyCtx ? PyObject_CallOneArg(function, pyCtx) : nullptr;
    if (value == nullptr) {
        result = failure(takeError());
    } else if (PyCoro_CheckExact(value)) {
        PyObject* closed = PyObject_CallMethod(value, "close", nullptr);
        Py_XDECREF(closed);
        PyErr_Clear();
        result = failure("NotImplementedError", "async 함수는 아직 지원하지 않습니다 (대기형 명령은 이후 단계에서 구현 예정)");
    } else if (returned != nullptr && !fromPython(value, *returned)) {
        ScriptError error = takeError();
        error.message = "반환값을 변수에 넣을 수 없습니다 — " + error.message;
        result = failure(std::move(error));
    }
    Py_XDECREF(value);
    Py_XDECREF(pyCtx);
    Py_XDECREF(naru);
    return result;
}

PyObject* newGlobals(const std::string& path) {
    PyObject* globals = PyDict_New();
    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
    PyObject* file = PyUnicode_FromString(path.c_str());
    PyDict_SetItemString(globals, "__file__", file);
    Py_DECREF(file);
    return globals;
}

void dropModule(const std::string& path) {
    auto it = g_modules.find(path);
    if (it == g_modules.end()) {
        return;
    }
    // import 등록도 지운다 (우리가 넣은 모듈일 때만).
    PyObject* modules = PyImport_GetModuleDict(); // borrowed
    std::string name = moduleNameOf(path);
    if (PyObject* registered = PyDict_GetItemString(modules, name.c_str()); registered == it->second) {
        PyDict_DelItemString(modules, name.c_str());
    }
    Py_DECREF(it->second);
    g_modules.erase(it);
    PyErr_Clear();
}

} // namespace

ScriptResult PythonRuntime::runFile(const std::string& path, CommandContext& ctx, const std::string& functionName,
                                    Value* returned) {
    ScriptScope scope(path, &ctx);
    ScriptResult result;

    PyObject* code = compileFile(path, result);
    if (code == nullptr) {
        logFailure(result, ctx);
        return result;
    }

    // 매 실행마다 새 전역 공간을 사용해 이전 실행의 상태가 남지 않게 한다.
    PyObject* globals = newGlobals(path);
    PyObject* name = PyUnicode_FromString("__naru_script__");
    PyDict_SetItemString(globals, "__name__", name);
    Py_DECREF(name);

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
            result = invoke(function, returned);
        }
    }
    flushStreams();
    Py_DECREF(globals);
    logFailure(result, ctx);
    return result;
}

ScriptResult PythonRuntime::loadModule(const std::string& path, CommandContext& ctx) {
    ScriptScope scope(path, &ctx);
    dropModule(path);
    ScriptResult result;
    PyObject* code = compileFile(path, result);
    if (code == nullptr) {
        logFailure(result, ctx);
        return result;
    }
    std::string name = moduleNameOf(path);
    PyObject* module = PyModule_New(name.c_str());
    PyObject* dict = module ? PyModule_GetDict(module) : nullptr; // borrowed
    if (dict == nullptr) {
        Py_XDECREF(module);
        Py_DECREF(code);
        result = failure(takeError());
        logFailure(result, ctx);
        return result;
    }
    PyDict_SetItemString(dict, "__builtins__", PyEval_GetBuiltins());
    PyObject* file = PyUnicode_FromString(path.c_str());
    PyDict_SetItemString(dict, "__file__", file);
    Py_DECREF(file);

    // 다른 스크립트에서 "import 이름"으로 같은 모듈을 쓰도록 등록한다 (최상위 코드 실행 전에: 순환 import 대비).
    PyObject* modules = PyImport_GetModuleDict(); // borrowed
    PyObject* existing = PyDict_GetItemString(modules, name.c_str()); // borrowed
    bool registered = existing == nullptr;
    if (registered) {
        PyDict_SetItemString(modules, name.c_str(), module);
    } else {
        ctx.logger.warning("전역 스크립트 '" + name + "'은(는) 이미 있는 Python 모듈과 이름이 같아 import로는 쓸 수 없습니다",
                           {path, 0, ""});
    }

    PyObject* executed = PyEval_EvalCode(code, dict, dict);
    Py_DECREF(code);
    if (executed == nullptr) {
        result = failure(takeError());
        if (registered) {
            PyDict_DelItemString(modules, name.c_str());
        }
        Py_DECREF(module);
    } else {
        Py_DECREF(executed);
        g_modules[path] = module;
    }
    flushStreams();
    logFailure(result, ctx);
    return result;
}

bool PythonRuntime::hasModule(const std::string& path) const {
    return g_modules.count(path) > 0;
}

CallStatus PythonRuntime::callModuleFunction(const std::string& path, const std::string& functionName,
                                             CommandContext& ctx, ScriptResult* resultOut, Value* returned) {
    auto it = g_modules.find(path);
    if (it == g_modules.end()) {
        return CallStatus::Missing;
    }
    PyObject* function = PyDict_GetItemString(PyModule_GetDict(it->second), functionName.c_str()); // borrowed
    if (function == nullptr || !PyCallable_Check(function)) {
        return CallStatus::Missing;
    }
    ScriptScope scope(path, &ctx);
    Py_INCREF(function); // 함수가 실행 중에 모듈에서 지워져도 안전하도록
    ScriptResult result = invoke(function, returned);
    Py_DECREF(function);
    flushStreams();
    logFailure(result, ctx);
    if (resultOut) {
        *resultOut = result;
    }
    return result.ok ? CallStatus::Ok : CallStatus::Failed;
}

void PythonRuntime::unloadModules() {
    while (!g_modules.empty()) {
        dropModule(g_modules.begin()->first);
    }
}

std::vector<ScriptError> PythonRuntime::checkGlobalScripts(const std::vector<std::string>& paths) {
    std::vector<ScriptError> errors;
    PyObject* naru = PyImport_ImportModule("naru");
    if (naru == nullptr) {
        PyErr_Clear();
        return errors;
    }
    // 이름 → 처음 나온 파일·행
    std::map<std::string, std::pair<std::string, int>> seen;
    for (const std::string& path : paths) {
        ScriptScope scope(path, nullptr);
        std::string name = moduleNameOf(path);
        PyObject* standard = PyObject_CallMethod(naru, "_is_standard_module", "s", name.c_str());
        if (standard == Py_True) {
            errors.push_back({"NameConflict",
                              "전역 스크립트 파일 이름 '" + name + "'이(가) Python 표준 모듈과 같습니다. 파일 이름을 바꾸세요",
                              path, 0, ""});
        }
        Py_XDECREF(standard);
        PyErr_Clear();

        std::string source;
        if (!readSource(path, source)) {
            errors.push_back({"FileNotFoundError", "전역 스크립트 파일이 없습니다", path, 0, ""});
            continue;
        }
        PyObject* names = PyObject_CallMethod(naru, "_global_names", "ss", path.c_str(), source.c_str());
        if (names == nullptr) {
            errors.push_back(takeError());
            continue;
        }
        for (Py_ssize_t i = 0; i < PyList_Size(names); ++i) {
            PyObject* pair = PyList_GetItem(names, i); // borrowed
            std::string variable = utf8(PyTuple_GetItem(pair, 0));
            int line = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(pair, 1)));
            auto [it, inserted] = seen.emplace(variable, std::make_pair(path, line));
            if (!inserted) {
                std::string first =
                    u8(std::filesystem::path(reinterpret_cast<const char8_t*>(it->second.first.c_str())).filename());
                errors.push_back({"NameConflict",
                                  "전역 스크립트 변수 이름이 겹칩니다: " + variable + " — " + first + ":" +
                                      std::to_string(it->second.second) + "에도 있습니다",
                                  path, line, ""});
            }
        }
        Py_DECREF(names);
    }
    Py_DECREF(naru);
    PyErr_Clear();
    return errors;
}

} // namespace naru::python