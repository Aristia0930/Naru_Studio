#include "TestMain.h"

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/events/BuiltinCommands.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/Session.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace naru;
using naru::python::PythonRuntime;

namespace {

PythonRuntime* g_runtime = nullptr;

std::string script(const char* name) {
    return std::string(NARU_TEST_SCRIPTS_DIR) + "/" + name;
}

struct Fixture {
    Variables variables;
    Logger logger;
    CommandContext ctx{variables, logger};
};

bool hasMessage(const Logger& logger, const std::string& text) {
    for (const auto& e : logger.entries()) {
        if (e.message == text) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("스크립트가 변수를 읽고 쓰며 로그를 남긴다") {
    Fixture f;
    f.variables.set("visits", std::int64_t{2});
    auto result = g_runtime->runFile(script("visits.py"), f.ctx);
    CHECK(result.ok);
    CHECK(f.variables.get("visits") == Value{std::int64_t{3}});
    CHECK(f.variables.get("greeting") == Value{std::string("안녕하세요")});
    CHECK(f.variables.get("ratio") == Value{0.5});
    CHECK(f.variables.get("flag") == Value{true});
    CHECK(hasMessage(f.logger, "방문 횟수: 3"));
    CHECK(hasMessage(f.logger, "print도 로그로 간다"));
    // ctx.log 호출 위치(행)가 기록된다.
    bool found = false;
    for (const auto& e : f.logger.entries()) {
        if (e.message == "방문 횟수: 3") {
            found = e.source.line == 5 && e.source.file == script("visits.py");
        }
    }
    CHECK(found);
}

TEST_CASE("문법 오류는 실행 없이 파일과 행을 보고한다") {
    auto result = g_runtime->checkSyntax(script("syntax_error.py"));
    CHECK(!result.ok);
    CHECK(result.error.type == "SyntaxError");
    CHECK(result.error.line == 3);
    CHECK(result.error.file == script("syntax_error.py"));
}

TEST_CASE("실행 오류는 사용자 스크립트의 행과 traceback을 보고한다") {
    Fixture f;
    auto result = g_runtime->runFile(script("runtime_error.py"), f.ctx);
    CHECK(!result.ok);
    CHECK(result.error.type == "ZeroDivisionError");
    CHECK(result.error.line == 2); // helper() 안의 1 / 0
    CHECK(result.error.traceback.find("Traceback") != std::string::npos);
    CHECK(result.error.traceback.find("helper") != std::string::npos);
    CHECK(f.logger.entries().back().level == LogLevel::Error);
    CHECK(f.logger.entries().back().source.line == 2);
}

TEST_CASE("지원하지 않는 값 형식은 TypeError가 된다") {
    Fixture f;
    auto result = g_runtime->runFile(script("bad_value.py"), f.ctx);
    if (result.error.type != "TypeError") {
        std::printf("  실제 오류: %s\n%s\n", result.summary().c_str(), result.error.traceback.c_str());
    }
    CHECK(!result.ok);
    CHECK(result.error.type == "TypeError");
    CHECK(result.error.line == 2);
}

TEST_CASE("run 함수가 없으면 오류를 보고한다") {
    Fixture f;
    auto result = g_runtime->runFile(script("no_run.py"), f.ctx);
    CHECK(!result.ok);
    CHECK(result.error.type == "NameError");
}

TEST_CASE("파일을 수정하면 다음 실행에 최신 코드가 반영된다") {
    auto path = std::filesystem::temp_directory_path() / "naru_reload_test.py";
    auto write = [&](const char* body) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << body;
    };
    std::string pathUtf8 = reinterpret_cast<const char*>(path.u8string().c_str());
    Fixture f;
    write("def run(ctx):\n    ctx.variables.set('v', 1)\n");
    CHECK(g_runtime->runFile(pathUtf8, f.ctx).ok);
    CHECK(f.variables.get("v") == Value{std::int64_t{1}});
    write("def run(ctx):\n    ctx.variables.set('v', 2)\n");
    CHECK(g_runtime->runFile(pathUtf8, f.ctx).ok);
    CHECK(f.variables.get("v") == Value{std::int64_t{2}});
    std::filesystem::remove(path);
}

TEST_CASE("이벤트 명령으로 스크립트를 호출하고 실패하면 이벤트가 멈춘다") {
    Session session(Variables{});
    std::vector<std::unique_ptr<Command>> list;
    auto call = [](std::string path) {
        return [path](CommandContext& ctx) {
            auto r = g_runtime->runFile(path, ctx);
            return r.ok ? std::string() : r.summary();
        };
    };
    list.push_back(std::make_unique<CallCommand>("ok", call(script("visits.py"))));
    list.push_back(std::make_unique<CallCommand>("bad", call(script("runtime_error.py"))));
    list.push_back(std::make_unique<SetVariableCommand>("after", "after", true));
    session.start(std::move(list));
    CHECK(session.state() == SessionState::Failed);
    CHECK(session.runner().failedCommandId() == "bad");
    CHECK(session.variables().get("visits") == Value{std::int64_t{1}});
    CHECK(!session.variables().contains("after"));
}

TEST_CASE("async 함수는 아직 지원하지 않는다고 명확히 보고한다") {
    Fixture f;
    auto result = g_runtime->runFile(script("async_run.py"), f.ctx);
    CHECK(!result.ok);
    CHECK(result.error.type == "NotImplementedError");
}

int main() {
    try {
        PythonRuntime runtime;
        g_runtime = &runtime;
        std::printf("Python %s\n\n", runtime.version().c_str());
        return naru::test::runAll();
    } catch (const std::exception& e) {
        std::printf("Python 초기화 실패: %s\n", e.what());
        return 2;
    }
}
