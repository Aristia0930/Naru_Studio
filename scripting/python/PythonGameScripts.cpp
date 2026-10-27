#include "naru/python/PythonGameScripts.h"

#include "naru/core/Log.h"

#include <filesystem>

namespace naru::python {

namespace {

std::string fileNameOf(const std::string& path) {
    std::u8string name = std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())).filename().u8string();
    return std::string(name.begin(), name.end());
}

} // namespace

PythonGameScripts::PythonGameScripts(PythonRuntime& python, std::vector<std::string> paths)
    : python_(python), paths_(std::move(paths)) {}

PythonGameScripts::~PythonGameScripts() {
    // 모듈은 게임 한 번 동안만 유지한다 (에디터에서 다시 실행하면 최신 파일로 새로 읽는다).
    python_.unloadModules();
}

int PythonGameScripts::load(CommandContext& ctx) {
    int errors = 0;
    for (const ScriptError& e : python_.checkGlobalScripts(paths_)) {
        LogEntry entry;
        entry.level = LogLevel::Error;
        entry.message = e.type + ": " + e.message;
        entry.source = {e.file, e.line, ""};
        entry.detail = e.traceback;
        ctx.logger.log(std::move(entry));
        ++errors;
    }
    loaded_.clear();
    failed_.clear();
    for (const std::string& path : paths_) {
        if (!std::filesystem::exists(std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str())))) {
            continue; // checkGlobalScripts가 이미 보고했다
        }
        if (python_.loadModule(path, ctx).ok) {
            loaded_.push_back(path);
        } else {
            ++errors;
        }
    }
    return errors;
}

HookStatus PythonGameScripts::call(const std::string& hook, CommandContext& ctx) {
    bool ran = false;
    bool failed = false;
    for (const std::string& path : loaded_) {
        if (failed_.count({path, hook})) {
            continue;
        }
        switch (python_.callModuleFunction(path, hook, ctx)) {
        case CallStatus::Ok: ran = true; break;
        case CallStatus::Missing: break;
        case CallStatus::Failed:
            failed_.insert({path, hook});
            failed = true;
            break;
        }
    }
    if (failed) {
        return HookStatus::Failed;
    }
    // 오류로 꺼진 스크립트만 남았거나 아무 스크립트에도 없으면 더 부를 필요가 없다.
    return ran ? HookStatus::Ok : HookStatus::Missing;
}

std::optional<std::string> PythonGameScripts::loadedPath(const std::string& fileName) const {
    for (const std::string& path : loaded_) {
        if (fileNameOf(path) == fileName) {
            return path;
        }
    }
    return std::nullopt;
}

} // namespace naru::python
