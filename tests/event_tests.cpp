#include "TestMain.h"

#include "naru/events/EventCompiler.h"
#include "naru/runtime/Session.h"

#include <string>
#include <vector>

using namespace naru;

namespace {

EventCommand say(std::string speaker, std::string text) {
    EventCommand c;
    c.type = EventCommandType::Say;
    c.speaker = std::move(speaker);
    c.text = std::move(text);
    return c;
}

EventCommand setVar(std::string name, VariableOp op, Value value) {
    EventCommand c;
    c.type = EventCommandType::SetVariable;
    c.variable = std::move(name);
    c.op = op;
    c.value = std::move(value);
    return c;
}

EventCommand ifVar(std::string name, CompareOp compare, Value value, std::vector<EventCommand> thenList,
                   std::vector<EventCommand> elseList = {}) {
    EventCommand c;
    c.type = EventCommandType::IfVariable;
    c.variable = std::move(name);
    c.compare = compare;
    c.value = std::move(value);
    c.thenCommands = std::move(thenList);
    c.elseCommands = std::move(elseList);
    return c;
}

EventCommand logCmd(std::string text) {
    EventCommand c;
    c.type = EventCommandType::Log;
    c.text = std::move(text);
    return c;
}

// 플레이어가 대사를 넘기는 것을 흉내 낸다.
struct FakeDialogue : DialogueHost {
    std::vector<std::string> shown;
    bool open = false;
    void showMessage(const std::string& speaker, const std::string& text) override {
        shown.push_back(speaker + ":" + text);
        open = true;
    }
    bool messageClosed() const override { return !open; }
    void closeMessage() override { open = false; }
};

std::vector<std::string> logs(const Session& s) {
    std::vector<std::string> out;
    for (const auto& e : const_cast<Session&>(s).logger().entries()) {
        out.push_back(e.message);
    }
    return out;
}

void run(Session& session, const std::vector<EventCommand>& commands, const ScriptRunner& runner = {}) {
    session.start(compileEvent(commands, runner));
}

} // namespace

TEST_CASE("대사는 플레이어가 넘길 때까지 기다린다") {
    FakeDialogue dialogue;
    Session session(Variables{}, 0.25, &dialogue);
    run(session, {say("촌장", "어서 오게"), say("", "..."), logCmd("끝")});
    CHECK(session.state() == SessionState::Running);
    CHECK(dialogue.shown.size() == 1 && dialogue.shown[0] == "촌장:어서 오게");
    session.advance(1.0);
    CHECK(dialogue.shown.size() == 1); // 넘기지 않으면 그대로
    dialogue.closeMessage();
    session.advance(0.25);
    CHECK(dialogue.shown.size() == 2);
    dialogue.closeMessage();
    session.advance(0.25);
    CHECK(session.state() == SessionState::Completed);
    CHECK(logs(session).back() == "끝");
}

TEST_CASE("대사 중 중지하면 대사 창을 닫는다") {
    FakeDialogue dialogue;
    Session session(Variables{}, 0.25, &dialogue);
    run(session, {say("a", "b")});
    CHECK(dialogue.open);
    session.stop();
    CHECK(!dialogue.open);
    CHECK(session.state() == SessionState::Stopped);
}

TEST_CASE("변수 대입·더하기·빼기") {
    Session session(Variables{});
    run(session, {
        setVar("gold", VariableOp::Add, std::int64_t{100}), // 없는 변수는 0에서 시작
        setVar("gold", VariableOp::Subtract, std::int64_t{30}),
        setVar("rate", VariableOp::Set, 1.5),
        setVar("rate", VariableOp::Add, std::int64_t{1}),
        setVar("name", VariableOp::Set, std::string("리아")),
        setVar("name", VariableOp::Add, std::string("님")),
    });
    CHECK(session.state() == SessionState::Completed);
    CHECK(session.variables().get("gold") == Value{std::int64_t{70}});
    CHECK(session.variables().get("rate") == Value{2.5});
    CHECK(session.variables().get("name") == Value{std::string("리아님")});
}

TEST_CASE("조건 분기: 그렇다면/아니라면 중 하나만 실행한다") {
    std::vector<EventCommand> event = {
        ifVar("visits", CompareOp::GreaterEqual, std::int64_t{3},
              {logCmd("단골"), setVar("bonus", VariableOp::Set, true)},
              {logCmd("처음")}),
        logCmd("공통"),
    };
    {
        Variables v;
        v.set("visits", std::int64_t{5});
        Session session(v);
        run(session, event);
        CHECK((logs(session) == std::vector<std::string>{"단골", "공통"}));
        CHECK(session.variables().get("bonus") == Value{true});
    }
    {
        Session session(Variables{});
        run(session, event); // 없는 변수를 숫자와 비교하면 0으로 본다
        CHECK((logs(session) == std::vector<std::string>{"처음", "공통"}));
    }
    {
        Variables v;
        v.set("visits", std::int64_t{1});
        Session session(v);
        run(session, event);
        CHECK((logs(session) == std::vector<std::string>{"처음", "공통"}));
    }
}

TEST_CASE("중첩 분기와 빈 아니라면 분기") {
    Variables v;
    v.set("a", std::int64_t{1});
    v.set("b", std::string("x"));
    Session session(v);
    run(session, {
        ifVar("a", CompareOp::Equal, std::int64_t{1},
              {ifVar("b", CompareOp::Equal, std::string("y"), {logCmd("안쪽")}), logCmd("바깥")}),
        logCmd("끝"),
    });
    CHECK((logs(session) == std::vector<std::string>{"바깥", "끝"}));
}

TEST_CASE("실패한 명령의 ID는 편집 화면의 위치와 같다") {
    Variables v;
    v.set("flag", true);
    v.set("text", std::string("abc"));
    Session session(v);
    run(session, {
        logCmd("시작"),
        ifVar("flag", CompareOp::Equal, true, {logCmd("ok"), setVar("text", VariableOp::Subtract, std::int64_t{1})}),
    });
    CHECK(session.state() == SessionState::Failed);
    CHECK(session.runner().failedCommandId() == "1/t/1");
    CHECK(session.runner().failureMessage().find("text") != std::string::npos);
}

TEST_CASE("스크립트 호출 명령은 주어진 실행기를 사용한다") {
    std::vector<std::string> called;
    ScriptRunner runner = [&called](const std::string& script, const std::string& function, CommandContext& ctx) {
        called.push_back(script + ":" + function);
        ctx.variables.set("from_script", std::int64_t{7});
        return script == "bad.py" ? std::string("스크립트 오류") : std::string();
    };
    EventCommand ok;
    ok.type = EventCommandType::CallScript;
    ok.script = "reward.py";
    EventCommand bad = ok;
    bad.script = "bad.py";
    bad.function = "give";

    Session session(Variables{});
    run(session, {ok, bad, logCmd("도달하면 안 됨")}, runner);
    CHECK((called == std::vector<std::string>{"reward.py:run", "bad.py:give"}));
    CHECK(session.variables().get("from_script") == Value{std::int64_t{7}});
    CHECK(session.state() == SessionState::Failed);
    CHECK(session.runner().failedCommandId() == "1");
}

TEST_CASE("주석은 실행하지 않고 대기는 시간을 기다린다") {
    EventCommand comment;
    comment.type = EventCommandType::Comment;
    comment.text = "메모";
    EventCommand wait;
    wait.type = EventCommandType::Wait;
    wait.seconds = 0.5;
    Session session(Variables{}, 0.25);
    run(session, {comment, wait, logCmd("후")});
    CHECK(logs(session).empty());
    session.advance(0.25);
    CHECK(session.state() == SessionState::Running);
    session.advance(0.25);
    CHECK(session.state() == SessionState::Completed);
}

TEST_CASE("값 비교와 연산 규칙") {
    std::string error;
    CHECK(compareValues(std::int64_t{2}, CompareOp::Less, 2.5, error) && error.empty());
    CHECK(compareValues(std::string("a"), CompareOp::Less, std::string("b"), error));
    CHECK(compareValues(Value{}, CompareOp::Equal, Value{}, error));
    CHECK(!compareValues(Value{}, CompareOp::Equal, std::int64_t{0}, error) && error.empty());
    compareValues(true, CompareOp::Greater, std::int64_t{1}, error);
    CHECK(!error.empty());
    CHECK(!applyVariableOp(true, VariableOp::Add, std::int64_t{1}, error).has_value());
}

TEST_MAIN()
