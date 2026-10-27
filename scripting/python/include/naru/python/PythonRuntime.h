#pragma once

#include "naru/core/Value.h"
#include "naru/events/Command.h"

#include <string>
#include <vector>

namespace naru::python {

// 스크립트 오류의 위치. file/line은 사용자 스크립트 안의 가장 가까운 프레임이다.
struct ScriptError {
    std::string type;
    std::string message;
    std::string file;
    int line = 0;
    std::string traceback;
};

struct ScriptResult {
    bool ok = true;
    ScriptError error;

    // "파일:행: 형식: 메시지" 한 줄 요약.
    std::string summary() const;
};

enum class CallStatus { Ok, Missing, Failed };

// 프로세스당 하나만 만들 수 있는 내장 CPython 인터프리터.
// 스크립트는 사용자 로컬 코드로 실행되며 격리(sandbox)되지 않는다.
//
// ctx로 쓸 수 있는 것: ctx.variables, ctx.db, ctx.log()/warning(), 그리고 게임 실행 중이면
// ctx.input, ctx.player, ctx.scene (게임 밖 — 에디터 테스트 실행 등 — 에서는 None).
class PythonRuntime {
public:
    // pythonHome이 비어 있으면 NARU_PYTHON_HOME 환경변수, 그다음 빌드 시 감지한 경로를 사용한다.
    // 초기화에 실패하면 std::runtime_error를 던진다.
    explicit PythonRuntime(const std::string& pythonHome = {});
    ~PythonRuntime();
    PythonRuntime(const PythonRuntime&) = delete;
    PythonRuntime& operator=(const PythonRuntime&) = delete;

    std::string version() const;

    // 실행하지 않고 문법만 검사한다.
    ScriptResult checkSyntax(const std::string& path);

    // 파일을 매번 새로 읽어 실행한 뒤 functionName(ctx)를 호출한다. 편집 후 재실행하면 최신 코드가 반영된다.
    // print()와 ctx.log()는 ctx.logger로 전달된다. returned가 있으면 반환값을 넣는다
    // (None·bool·int·float·str가 아니면 TypeError).
    ScriptResult runFile(const std::string& path, CommandContext& ctx, const std::string& functionName = "run",
                         Value* returned = nullptr);

    // ---- 전역 스크립트 모듈: 한 번 실행해 두고 함수를 여러 번 부른다. 모듈 전역 변수가 유지되며,
    // 다른 스크립트에서 파일 이름(확장자 제외)으로 import할 수 있다.
    // 같은 경로를 다시 읽으면 이전 모듈을 버린다. 실패하면 모듈을 남기지 않는다.
    ScriptResult loadModule(const std::string& path, CommandContext& ctx);
    bool hasModule(const std::string& path) const;
    // 모듈에 함수가 없으면 Missing (로그 없음). 오류면 Failed와 함께 result·로그에 오류를 남긴다.
    CallStatus callModuleFunction(const std::string& path, const std::string& functionName, CommandContext& ctx,
                                  ScriptResult* result = nullptr, Value* returned = nullptr);
    void unloadModules();

    // 전역 스크립트 검사: 문법 오류, 파일 최상위 변수 이름이 여러 전역 스크립트에서 겹침,
    // 모듈 이름이 Python 표준 모듈과 겹침. 문제가 없으면 빈 목록.
    // (훅 함수, import한 이름, _로 시작하는 이름은 겹쳐도 된다)
    std::vector<ScriptError> checkGlobalScripts(const std::vector<std::string>& paths);
};

} // namespace naru::python
