#include "TestMain.h"

#include "naru/events/BuiltinCommands.h"
#include "naru/runtime/Session.h"

#include <memory>
#include <string>
#include <vector>

using namespace naru;

namespace {

std::vector<std::unique_ptr<Command>> commands(std::vector<Command*> raw) {
    std::vector<std::unique_ptr<Command>> list;
    for (Command* c : raw) {
        list.emplace_back(c);
    }
    return list;
}

std::vector<std::string> messages(const Logger& logger) {
    std::vector<std::string> result;
    for (const auto& e : logger.entries()) {
        result.push_back(e.message);
    }
    return result;
}

} // namespace

TEST_CASE("즉시 명령은 순서대로 한 번에 실행된다") {
    Variables doc;
    Session session(doc);
    session.start(commands({
        new LogCommand("c1", "first"),
        new SetVariableCommand("c2", "hp", std::int64_t{10}),
        new LogCommand("c3", "second"),
    }));
    CHECK(session.state() == SessionState::Completed);
    CHECK((messages(session.logger()) == std::vector<std::string>{"first", "second"}));
    CHECK(session.variables().get("hp") == Value{std::int64_t{10}});
}

TEST_CASE("대기 명령은 고정 틱이 쌓일 때까지 다음 명령을 막는다") {
    // 2진수로 정확히 표현되는 값을 써서 부동소수점 오차 없이 틱 수를 검증한다.
    Session session(Variables{}, 0.25);
    session.start(commands({
        new LogCommand("a", "before"),
        new WaitCommand("w", 0.625),
        new LogCommand("b", "after"),
    }));
    CHECK(session.state() == SessionState::Running);
    CHECK(session.runner().status() == CommandStatus::Waiting);
    CHECK(session.advance(0.375) == 1); // 1틱 실행, 0.125초 이월
    CHECK(session.logger().entries().size() == 1);
    session.advance(0.375); // 누적 0.5초 → 2틱 더, 3틱째에 대기 종료
    CHECK(session.state() == SessionState::Completed);
    CHECK(session.tickCount() == 3);
    CHECK(messages(session.logger()).back() == "after");
}

TEST_CASE("대기 중 중지하면 이후 명령을 실행하지 않는다") {
    Session session(Variables{}, 0.1);
    session.start(commands({
        new WaitCommand("w", 1.0),
        new SetVariableCommand("s", "reached", true),
    }));
    session.advance(0.2);
    session.stop();
    CHECK(session.state() == SessionState::Stopped);
    CHECK(session.runner().status() == CommandStatus::Cancelled);
    CHECK(session.advance(5.0) == 0);
    CHECK(!session.variables().contains("reached"));
}

TEST_CASE("명령이 실패하면 다음 명령으로 넘어가지 않고 위치를 기록한다") {
    Session session(Variables{});
    session.start(commands({
        new SetVariableCommand("s1", "step", std::int64_t{1}),
        new CallCommand("boom", [](CommandContext&) { return std::string("테스트 실패"); }),
        new SetVariableCommand("s2", "step", std::int64_t{2}),
    }));
    CHECK(session.state() == SessionState::Failed);
    CHECK(session.runner().failedCommandId() == "boom");
    CHECK(session.runner().failureMessage() == "테스트 실패");
    CHECK(session.variables().get("step") == Value{std::int64_t{1}});
    const auto& last = session.logger().entries().back();
    CHECK(last.level == LogLevel::Error);
    CHECK(last.source.commandId == "boom");
}

TEST_CASE("선택한 명령부터 실행할 수 있다") {
    Session session(Variables{});
    session.start(commands({
        new LogCommand("a", "skip"),
        new LogCommand("b", "run"),
    }), 1);
    CHECK((messages(session.logger()) == std::vector<std::string>{"run"}));
}

TEST_CASE("실행 세션의 변경은 편집 문서의 변수에 남지 않는다") {
    Variables doc;
    doc.set("gold", std::int64_t{100});
    Variables snapshot = doc;
    {
        Session session(doc);
        session.start(commands({new SetVariableCommand("s", "gold", std::int64_t{0})}));
        CHECK(session.variables().get("gold") == Value{std::int64_t{0}});
    }
    CHECK(doc == snapshot);
}

TEST_CASE("긴 멈춤 뒤에도 한 번에 처리하는 틱 수는 제한된다") {
    Session session(Variables{}, 0.01);
    session.start(commands({new WaitCommand("w", 100.0)}));
    CHECK(session.advance(10.0) == 8);
    CHECK(session.advance(0.0) == 0);
}

TEST_MAIN()
