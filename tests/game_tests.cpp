#include "TestMain.h"

#include "naru/runtime/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using namespace naru;

namespace {

constexpr double kStep = 1.0 / 60.0;

EventCommand say(std::string text) {
    EventCommand c;
    c.type = EventCommandType::Say;
    c.speaker = "안내";
    c.text = std::move(text);
    return c;
}

EventCommand add(std::string name, std::int64_t amount) {
    EventCommand c;
    c.type = EventCommandType::SetVariable;
    c.variable = std::move(name);
    c.op = VariableOp::Add;
    c.value = amount;
    return c;
}

EventCommand wait(double seconds) {
    EventCommand c;
    c.type = EventCommandType::Wait;
    c.seconds = seconds;
    return c;
}

SceneObject object(std::string id, EventTrigger trigger, std::vector<EventCommand> commands) {
    SceneObject o;
    o.id = id;
    o.name = id;
    o.event.trigger = trigger;
    o.event.commands = std::move(commands);
    return o;
}

Project projectWith(std::vector<SceneObject> objects) {
    Project p;
    p.name = "test";
    Scene s;
    s.id = "scene_001";
    s.name = "처음";
    s.objects = std::move(objects);
    p.scenes.push_back(s);
    p.startSceneId = s.id;
    return p;
}

ScriptRunner noScripts() {
    return [](const std::string&, const std::string&, CommandContext&) { return ScriptCallResult{"스크립트 없음", {}}; };
}

std::int64_t intVar(GameWorld& world, const std::string& name) {
    std::optional<Value> v = world.variables().get(name);
    return v && std::holds_alternative<std::int64_t>(*v) ? std::get<std::int64_t>(*v) : -1;
}

void step(GameWorld& world, const InputState& input = {}) {
    world.update(kStep, input);
}

InputState confirm() {
    InputState input;
    input.press(InputAction::Confirm);
    return input;
}

} // namespace

TEST_CASE("missing scene is reported") {
    GameWorld world(projectWith({}), noScripts());
    CHECK(!world.enterScene("nope"));
    CHECK(world.scene() == nullptr);
    CHECK(!world.logger().entries().empty());
}

TEST_CASE("autorun events run once, one at a time, and wait for dialogue") {
    GameWorld world(projectWith({
                        object("a", EventTrigger::Autorun, {add("n", 1), say("첫 번째")}),
                        object("b", EventTrigger::Autorun, {add("n", 10), say("두 번째")}),
                        object("c", EventTrigger::Action, {add("n", 100)}),
                    }),
                    noScripts());
    CHECK(world.enterScene("scene_001"));
    CHECK(world.blockingEventActive());

    step(world);
    CHECK(intVar(world, "n") == 1);
    CHECK(world.message().has_value() && world.message()->text == "첫 번째");

    // 넘기기 전까지는 다음 자동 실행이 시작되지 않는다.
    for (int i = 0; i < 30; ++i) {
        step(world);
    }
    CHECK(intVar(world, "n") == 1);

    step(world, confirm());
    CHECK(intVar(world, "n") == 11);
    CHECK(world.message().has_value() && world.message()->text == "두 번째");

    step(world, confirm());
    CHECK(!world.message().has_value());
    CHECK(!world.blockingEventActive());
    // 조사 이벤트는 자동으로 실행되지 않는다.
    CHECK(intVar(world, "n") == 11);
}

TEST_CASE("parallel events run alongside a blocking autorun") {
    GameWorld world(projectWith({
                        object("talk", EventTrigger::Autorun, {say("멈춰 있는 동안")}),
                        object("timer", EventTrigger::Parallel, {wait(0.25), add("t", 1), wait(0.25), add("t", 1)}),
                    }),
                    noScripts());
    world.enterScene("scene_001");
    for (int i = 0; i < 60; ++i) {
        step(world);
    }
    CHECK(world.message().has_value());
    CHECK(intVar(world, "t") == 2);
    CHECK(world.runningEventCount() == 1);
}

TEST_CASE("a failing event is logged with its object and does not stop others") {
    EventCommand call;
    call.type = EventCommandType::CallScript;
    call.script = "missing.py";
    GameWorld world(projectWith({
                        object("broken", EventTrigger::Autorun, {call}),
                        object("next", EventTrigger::Autorun, {add("n", 1)}),
                    }),
                    noScripts());
    world.enterScene("scene_001");
    step(world);
    CHECK(world.failedEventCount() == 1);
    CHECK(intVar(world, "n") == 1);

    bool found = false;
    for (const LogEntry& e : world.logger().entries()) {
        if (e.level == LogLevel::Error && e.message.find("broken") != std::string::npos && e.source.commandId == "0") {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("re-entering a scene restarts its events and clears dialogue") {
    GameWorld world(projectWith({object("a", EventTrigger::Autorun, {add("n", 1), say("안녕")})}), noScripts());
    world.enterScene("scene_001");
    step(world);
    CHECK(world.message().has_value());
    world.enterScene("scene_001");
    CHECK(!world.message().has_value());
    step(world);
    CHECK(intVar(world, "n") == 2);
}

TEST_CASE("input state tracks new presses per frame") {
    InputState input;
    input.press(InputAction::Left);
    CHECK(input.isHeld(InputAction::Left) && input.wasPressed(InputAction::Left));
    input.endFrame();
    input.press(InputAction::Left); // 키 반복은 새로 누름이 아니다
    CHECK(input.isHeld(InputAction::Left) && !input.wasPressed(InputAction::Left));
    input.release(InputAction::Left);
    CHECK(!input.isHeld(InputAction::Left));
}

// ---------------------------------------------------------------- 플레이어 조작

namespace {

SceneObject box(std::string id, double x, double y, double w, double h) {
    SceneObject o;
    o.id = id;
    o.name = id;
    o.x = x;
    o.y = y;
    o.width = w;
    o.height = h;
    return o;
}

// 플레이어(0,0 크기 32)와 다른 객체들이 있는 400x300 장면.
Project playerProject(std::vector<SceneObject> others) {
    std::vector<SceneObject> objects{box("hero", 0, 0, 32, 32)};
    for (auto& o : others) {
        objects.push_back(std::move(o));
    }
    Project p = projectWith(std::move(objects));
    p.scenes[0].width = 400;
    p.scenes[0].height = 300;
    p.scenes[0].playerObjectId = "hero";
    return p;
}

InputState holding(InputAction action) {
    InputState input;
    input.press(action);
    return input;
}

void hold(GameWorld& world, InputAction action, int ticks) {
    InputState input = holding(action);
    for (int i = 0; i < ticks; ++i) {
        world.update(kStep, input);
        input.endFrame();
    }
}

} // namespace

TEST_CASE("the player moves with direction input and stops at scene edges") {
    GameWorld world(playerProject({}), noScripts());
    world.enterScene("scene_001");
    world.setPlayerSpeed(120.0);
    hold(world, InputAction::Right, 60); // 1초
    CHECK(std::abs(world.player()->x - 120.0) < 0.01);
    CHECK(world.player()->y == 0.0);
    CHECK(world.facing() == Facing::Right);

    hold(world, InputAction::Left, 600);
    CHECK(world.player()->x == 0.0);
    hold(world, InputAction::Down, 600);
    CHECK(world.player()->y == 300.0 - 32.0);
    CHECK(world.facing() == Facing::Down);
}

TEST_CASE("solid objects block the player, passable ones do not") {
    SceneObject wall = box("wall", 100, 0, 20, 32);
    SceneObject floor = box("floor", 0, 100, 32, 20);
    floor.passable = true;
    GameWorld world(playerProject({wall, floor}), noScripts());
    world.enterScene("scene_001");
    hold(world, InputAction::Right, 120);
    CHECK(world.player()->x == 100.0 - 32.0);
    hold(world, InputAction::Down, 120);
    CHECK(world.player()->y > 120.0); // 바닥 위를 지나갔다
}

TEST_CASE("touch events run once per bump and once per step onto a passable object") {
    SceneObject wall = box("wall", 100, 0, 20, 32);
    wall.event.trigger = EventTrigger::Touch;
    wall.event.commands = {add("bump", 1)};
    SceneObject trap = box("trap", 0, 100, 32, 20);
    trap.passable = true;
    trap.event.trigger = EventTrigger::EventTouch;
    trap.event.commands = {add("step", 1)};
    GameWorld world(playerProject({wall, trap}), noScripts());
    world.enterScene("scene_001");

    hold(world, InputAction::Right, 120); // 벽까지 가서 계속 민다
    CHECK(intVar(world, "bump") == 1);
    step(world);                          // 손을 뗐다가
    hold(world, InputAction::Right, 10);  // 다시 밀면 한 번 더
    CHECK(intVar(world, "bump") == 2);

    hold(world, InputAction::Left, 120);
    hold(world, InputAction::Down, 5);    // 아직 함정 위가 아님
    CHECK(intVar(world, "step") == -1);
    // 함정(100..120) 위로 들어갔다가 그 위에 머문다
    hold(world, InputAction::Down, 20);
    CHECK(intVar(world, "step") == 1);
    for (int i = 0; i < 30; ++i) {
        step(world);
    }
    CHECK(intVar(world, "step") == 1);
}

TEST_CASE("confirm runs the action event of the object the player faces") {
    SceneObject chest = box("chest", 40, 0, 32, 32); // 플레이어 오른쪽 8px 앞
    chest.event.commands = {say("상자다"), add("opened", 1)};
    SceneObject below = box("sign", 0, 200, 32, 32); // 멀리 있음
    below.event.commands = {add("sign", 1)};
    GameWorld world(playerProject({chest, below}), noScripts());
    world.enterScene("scene_001");

    // 아래를 보고 있으면 상자를 조사하지 않는다
    step(world, confirm());
    CHECK(!world.message().has_value());
    CHECK(intVar(world, "sign") == -1);

    hold(world, InputAction::Right, 1); // 오른쪽을 본다 (4px 이동, 상자 바로 앞)
    step(world, confirm());
    CHECK(world.message().has_value() && world.message()->text == "상자다");
    CHECK(world.blockingEventActive());

    // 대사가 떠 있는 동안은 움직이지 않는다
    double x = world.player()->x;
    hold(world, InputAction::Left, 30);
    CHECK(world.player()->x == x);

    // 대사를 넘긴 결정 키가 상자를 다시 조사하지 않는다
    step(world, confirm());
    CHECK(!world.message().has_value());
    CHECK(intVar(world, "opened") == 1);
    step(world);
    CHECK(!world.blockingEventActive());
    step(world, confirm());
    CHECK(world.message().has_value()); // 새로 누르면 다시 조사한다
}

TEST_CASE("a scene without a player has no player control") {
    GameWorld world(projectWith({box("a", 0, 0, 10, 10)}), noScripts());
    world.enterScene("scene_001");
    hold(world, InputAction::Right, 30);
    CHECK(world.player() == nullptr);
    CHECK(world.scene()->objects[0].x == 0.0);
}

// ---------------------------------------------------------------- 전역 스크립트 훅

namespace {

// 전역 스크립트 대역: 불린 훅을 기록하고, 훅마다 할 일을 넣을 수 있다.
struct FakeScripts : GameScripts {
    std::vector<std::string> calls;
    std::function<HookStatus(const std::string&, CommandContext&)> body;
    int load(CommandContext&) override {
        calls.push_back("load");
        return 0;
    }
    HookStatus call(const std::string& hook, CommandContext& ctx) override {
        calls.push_back(hook);
        return body ? body(hook, ctx) : HookStatus::Ok;
    }
    int count(const std::string& hook) const {
        return static_cast<int>(std::count(calls.begin(), calls.end(), hook));
    }
};

} // namespace

TEST_CASE("global hooks run in order: load, game start, scene enter, update, scene exit") {
    Project p = playerProject({});
    Scene second = p.scenes[0];
    second.id = "scene_002";
    second.name = "둘째";
    p.scenes.push_back(second);
    FakeScripts scripts;
    GameWorld world(p, noScripts());
    world.setGameScripts(&scripts);
    CHECK(world.startGame("scene_001"));
    CHECK((scripts.calls == std::vector<std::string>{"load", "on_game_start", "on_scene_enter"}));
    step(world);
    CHECK(scripts.calls.back() == "on_update");
    world.requestScene("scene_002");
    CHECK(world.scene()->id == "scene_001"); // 틱이 끝난 뒤에 바뀐다
    step(world);
    CHECK(world.scene()->id == "scene_002");
    auto n = scripts.calls.size();
    CHECK(scripts.calls[n - 2] == "on_scene_exit" && scripts.calls[n - 1] == "on_scene_enter");
}

TEST_CASE("a missing hook is not called again, a failing hook is counted") {
    FakeScripts scripts;
    scripts.body = [](const std::string& hook, CommandContext&) {
        return hook == "on_update" ? HookStatus::Missing
               : hook == "on_scene_enter" ? HookStatus::Failed
                                          : HookStatus::Ok;
    };
    GameWorld world(playerProject({}), noScripts());
    world.setGameScripts(&scripts);
    world.startGame("scene_001");
    for (int i = 0; i < 5; ++i) {
        step(world);
    }
    CHECK(scripts.count("on_update") == 1);
    CHECK(world.failedEventCount() == 1);
}

TEST_CASE("hooks share game variables with events and can drive the player") {
    SceneObject wall = box("wall", 100, 0, 20, 32);
    wall.event.trigger = EventTrigger::Touch;
    wall.event.commands = {add("bump", 1)};
    FakeScripts scripts;
    scripts.body = [](const std::string& hook, CommandContext& ctx) {
        if (hook == "on_game_start") {
            ctx.variables.set("n", std::int64_t{10});
            ctx.game->setDefaultControl(false); // 기본 조작 끄기
        } else if (hook == "on_update") {
            ctx.game->movePlayer(8, 0);         // 스크립트가 직접 이동 (충돌·접촉 판정 포함)
        }
        return HookStatus::Ok;
    };
    SceneObject counter = box("counter", 300, 200, 10, 10);
    counter.event.trigger = EventTrigger::Autorun;
    counter.event.commands = {add("n", 1)};
    GameWorld world(playerProject({wall, counter}), noScripts());
    world.setGameScripts(&scripts);
    world.startGame("scene_001");
    hold(world, InputAction::Left, 30); // 기본 조작이 꺼져 있어 왼쪽 입력은 무시된다
    CHECK(intVar(world, "n") == 11);    // 훅이 넣은 값에 이벤트가 더했다
    CHECK(world.player()->x == 100.0 - 32.0);
    CHECK(intVar(world, "bump") == 1);  // 스크립트 이동으로 부딪혀도 접촉 이벤트는 한 번
}

TEST_CASE("custom input actions: held every tick, pressed only on the first tick of a frame") {
    FakeScripts scripts;
    int held = 0;
    int pressed = 0;
    scripts.body = [&](const std::string& hook, CommandContext& ctx) {
        if (hook == "on_update") {
            held += ctx.game->input().isHeld("dash") ? 1 : 0;
            pressed += ctx.game->input().wasPressed("dash") ? 1 : 0;
        }
        return HookStatus::Ok;
    };
    GameWorld world(playerProject({}), noScripts());
    world.setGameScripts(&scripts);
    world.startGame("scene_001");
    world.inputMap().bind("dash", {"Shift"});
    CHECK((world.inputMap().actionsForKey("Left Shift") == std::vector<std::string>{"dash"}));
    CHECK(InputMap::keyMatches("Enter", "Return") && !InputMap::keyMatches("Shift", "Left Ctrl"));
    InputState input;
    input.press("dash");
    world.update(3.0 / 60.0, input); // 한 프레임에 3틱
    CHECK(held == 3);
    CHECK(pressed == 1);
}

TEST_MAIN()