#pragma once

#include "naru/events/Command.h"

#include <string>

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

// 프로세스당 하나만 만들 수 있는 내장 CPython 인터프리터.
// 스크립트는 사용자 로컬 코드로 실행되며 격리(sandbox)되지 않는다.
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
    // print()와 ctx.log()는 ctx.logger로 전달된다.
    ScriptResult runFile(const std::string& path, CommandContext& ctx,
                         const std::string& functionName = "run");
};

} // namespace naru::python
