#include "TestMain.h"

#include "naru/runtime/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <utility>
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

TEST_CASE("게임은 프로젝트 공용 변수의 처음 값으로 시작하고 장면이 바뀌어도 유지한다") {
    EventCommand ifClosed;
    ifClosed.type = EventCommandType::IfVariable;
    ifClosed.variable = "door_open";
    ifClosed.compare = CompareOp::Equal;
    ifClosed.value = false;
    ifClosed.thenCommands = {add("closed_seen", 1)};
    EventCommand go;
    go.type = EventCommandType::ChangeScene;
    go.scene = "scene_002";
    go.setPosition = false;
    Project p = projectWith({object("a", EventTrigger::Autorun, {add("gold", 5), ifClosed, go})});
    Scene second;
    second.id = "scene_002";
    second.name = "둘째";
    p.scenes.push_back(second);
    p.variables = {{"gold", std::int64_t{100}, "가진 돈"}, {"door_open", false, ""}};

    GameWorld world(std::move(p), noScripts());
    CHECK(world.variables().get("gold") == std::optional<Value>{std::int64_t{100}});
    CHECK(world.enterScene("scene_001"));
    for (int i = 0; i < 5; ++i) {
        step(world);
    }
    CHECK(world.scene()->id == "scene_002");
    CHECK(intVar(world, "gold") == 105);
    CHECK(intVar(world, "closed_seen") == 1);
    CHECK(world.variables().get("door_open") == std::optional<Value>{false});
}

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

// ---------------------------------------------------------------- 다른 객체 움직이기

TEST_CASE("objects move with the same collision rules as the player") {
    SceneObject crate = box("crate", 200, 0, 32, 32);
    SceneObject wall = box("wall", 300, 0, 20, 32);
    SceneObject ghost = box("ghost", 200, 100, 32, 32);
    ghost.passable = true; // 통과 가능한 객체는 막히지도, 막지도 않는다
    GameWorld world(playerProject({crate, wall, ghost}), noScripts());
    world.enterScene("scene_001");
    CHECK(world.moveObject("crate", 500, 0));
    CHECK(world.object("crate")->x == 300.0 - 32.0); // 벽에 막힘
    CHECK(world.moveObject("ghost", 500, 0));
    CHECK(world.object("ghost")->x == 400.0 - 32.0); // 벽은 지나가고 장면 가장자리에서 멈춤
    CHECK(!world.moveObject("nope", 1, 0));
    CHECK(world.moveObject("hero", 0, 10)); // 플레이어 ID면 플레이어 이동과 같다 (방향도 바뀜)
    CHECK(world.player()->y == 10.0);
    CHECK(world.facing() == Facing::Down);
    CHECK(world.setObjectPosition("crate", 5, 200) && world.object("crate")->x == 5.0);
}

TEST_CASE("an object that moves into the player runs its event-touch event once") {
    SceneObject slime = box("slime", 100, 0, 32, 32); // 이벤트 접촉
    slime.event.trigger = EventTrigger::EventTouch;
    slime.event.commands = {add("caught", 1)};
    SceneObject sign = box("sign", 100, 100, 32, 32); // 플레이어 접촉: 객체가 와서 닿으면 실행하지 않는다
    sign.event.trigger = EventTrigger::Touch;
    sign.event.commands = {add("sign", 1)};
    GameWorld world(playerProject({slime, sign}), noScripts());
    world.enterScene("scene_001");
    world.setPlayerPosition(0, 100);
    for (int i = 0; i < 20; ++i) {
        world.moveObject("sign", -8, 0); // 플레이어(0..32) 쪽으로 계속 민다
        step(world);
    }
    CHECK(world.object("sign")->x == 32.0);
    CHECK(intVar(world, "sign") == -1);

    world.setPlayerPosition(0, 0);
    for (int i = 0; i < 20; ++i) {
        world.moveObject("slime", -8, 0);
        step(world);
    }
    CHECK(world.object("slime")->x == 32.0);
    CHECK(intVar(world, "caught") == 1); // 계속 밀어도 한 번
    step(world);                          // 멈췄다가
    world.moveObject("slime", -8, 0);     // 다시 밀면 한 번 더
    step(world);
    CHECK(intVar(world, "caught") == 2);
}

TEST_CASE("a passable event-touch object that moves onto the player runs once") {
    SceneObject ghost = box("ghost", 200, 0, 32, 32);
    ghost.passable = true;
    ghost.event.trigger = EventTrigger::EventTouch;
    ghost.event.commands = {add("boo", 1)};
    GameWorld world(playerProject({ghost}), noScripts());
    world.enterScene("scene_001");
    for (int i = 0; i < 30; ++i) {
        world.moveObject("ghost", -8, 0); // 플레이어를 지나쳐 간다
        step(world);
    }
    CHECK(world.object("ghost")->x == 0.0);
    CHECK(intVar(world, "boo") == 1);
}

TEST_MAIN()
// ---------------------------------------------------------------- 이벤트 명령: 객체 이동·보이기/숨기기

namespace {

EventCommand moveCmd(std::string target, std::string direction, double distance, double speed, bool wait = true) {
    EventCommand c;
    c.type = EventCommandType::MoveObject;
    c.target = std::move(target);
    c.direction = std::move(direction);
    c.distance = distance;
    c.speed = speed;
    c.waitForEnd = wait;
    return c;
}

EventCommand showCmd(std::string target, bool show) {
    EventCommand c;
    c.type = EventCommandType::SetVisible;
    c.target = std::move(target);
    c.show = show;
    return c;
}

} // namespace

TEST_CASE("move command walks at its speed and the event waits until it arrives") {
    SceneObject npc = box("npc", 100, 100, 32, 32);
    npc.event.trigger = EventTrigger::Autorun;
    npc.event.commands = {moveCmd("@self", "right", 60, 120), add("arrived", 1)};
    GameWorld world(playerProject({npc}), noScripts());
    world.enterScene("scene_001");
    for (int i = 0; i < 15; ++i) {
        step(world);
    }
    CHECK(std::abs(world.object("npc")->x - 130.0) < 0.01); // 0.25초 × 120
    CHECK(intVar(world, "arrived") == -1);
    CHECK(world.blockingEventActive());
    for (int i = 0; i < 16; ++i) {
        step(world);
    }
    CHECK(std::abs(world.object("npc")->x - 160.0) < 0.01);
    CHECK(intVar(world, "arrived") == 1);
    CHECK(!world.objectMoving("npc"));
}

TEST_CASE("a move without waiting lets the event go on, and a blocked move stops where it is") {
    SceneObject wall = box("wall", 200, 100, 20, 32);
    SceneObject npc = box("npc", 100, 100, 32, 32);
    npc.event.trigger = EventTrigger::Autorun;
    npc.event.commands = {moveCmd("@self", "right", 500, 600, false), add("next", 1)};
    SceneObject cart = box("cart", 100, 200, 32, 32);
    cart.event.trigger = EventTrigger::Parallel;
    cart.event.commands = {moveCmd("@self", "right", 1000, 6000), add("cart_done", 1)}; // 장면 가장자리에서 멈춤
    GameWorld world(playerProject({wall, npc, cart}), noScripts());
    world.enterScene("scene_001");
    step(world);
    CHECK(intVar(world, "next") == 1); // 기다리지 않음
    CHECK(world.objectMoving("npc"));
    for (int i = 0; i < 30; ++i) {
        step(world);
    }
    CHECK(world.object("npc")->x == 200.0 - 32.0);
    CHECK(!world.objectMoving("npc"));
    CHECK(world.object("cart")->x == 400.0 - 32.0);
    CHECK(intVar(world, "cart_done") == 1);
}

TEST_CASE("move command can move the player or a named object; a missing target fails the event") {
    SceneObject npc = box("npc", 100, 100, 32, 32);
    SceneObject director = box("director", 300, 250, 10, 10);
    director.event.trigger = EventTrigger::Autorun;
    director.event.commands = {moveCmd("@player", "down", 40, 0), moveCmd("npc", "up", 30, 0)};
    SceneObject broken = box("broken", 300, 200, 10, 10);
    broken.event.trigger = EventTrigger::Autorun;
    broken.event.commands = {moveCmd("ghost", "up", 30, 0)};
    GameWorld world(playerProject({npc, director, broken}), noScripts());
    world.enterScene("scene_001");
    step(world);
    CHECK(world.player()->y == 40.0);
    CHECK(world.facing() == Facing::Down);
    CHECK(world.object("npc")->y == 70.0);
    CHECK(world.failedEventCount() == 1);
}

TEST_CASE("hidden objects are not solid, not interactable, and do not trigger contact") {
    SceneObject wall = box("wall", 100, 0, 20, 32);
    wall.visible = false;
    SceneObject chest = box("chest", 40, 100, 32, 32);
    chest.visible = false;
    chest.event.commands = {add("opened", 1)};
    SceneObject trap = box("trap", 0, 200, 32, 20);
    trap.passable = true;
    trap.visible = false;
    trap.event.trigger = EventTrigger::Touch;
    trap.event.commands = {add("trap", 1)};
    SceneObject hiddenTimer = box("timer", 300, 250, 10, 10);
    hiddenTimer.visible = false;
    hiddenTimer.event.trigger = EventTrigger::Parallel; // 숨겨도 병렬 처리는 실행된다
    hiddenTimer.event.commands = {add("tick", 1)};
    GameWorld world(playerProject({wall, chest, trap, hiddenTimer}), noScripts());
    world.enterScene("scene_001");
    hold(world, InputAction::Right, 60);
    CHECK(world.player()->x > 120.0); // 숨긴 벽은 지나간다
    CHECK(intVar(world, "tick") == 1);

    world.setPlayerPosition(0, 100);
    hold(world, InputAction::Right, 1); // 숨긴 상자를 바라본다
    step(world, confirm());
    CHECK(intVar(world, "opened") == -1);

    world.setPlayerPosition(0, 160);
    hold(world, InputAction::Down, 30); // 숨긴 함정 위를 지난다
    CHECK(intVar(world, "trap") == -1);
}

TEST_CASE("show/hide command changes visibility during the game") {
    SceneObject wall = box("wall", 100, 0, 20, 32);
    wall.visible = false;
    SceneObject lever = box("lever", 0, 40, 32, 32);
    lever.event.commands = {showCmd("wall", true), showCmd("@self", false)};
    GameWorld world(playerProject({wall, lever}), noScripts());
    world.enterScene("scene_001");
    step(world, confirm()); // 아래(레버)를 보고 조사
    step(world);
    CHECK(world.object("wall")->visible);
    CHECK(!world.object("lever")->visible);
    hold(world, InputAction::Right, 60);
    CHECK(world.player()->x == 100.0 - 32.0); // 다시 보이는 벽에 막힌다
}

// ---------------------------------------------------------------- 이벤트 명령: 장면 이동

namespace {

EventCommand changeScene(std::string scene, std::optional<std::pair<double, double>> position, std::string direction) {
    EventCommand c;
    c.type = EventCommandType::ChangeScene;
    c.scene = std::move(scene);
    c.setPosition = position.has_value();
    if (position) {
        c.x = position->first;
        c.y = position->second;
    }
    c.direction = std::move(direction);
    return c;
}

// scene_001(문이 있는 장면)과 scene_002(주인공이 (200, 100)에 놓인 장면, 들어오면 자동 실행으로 arrived += 1).
Project twoScenes(EventCommand doorCommand) {
    SceneObject door = box("door", 40, 0, 32, 32); // 주인공 오른쪽 바로 앞 (오른쪽을 보고 조사)
    door.event.commands = {add("before", 1), doorCommand, add("after", 1)};
    Project p = playerProject({door});
    Scene inside = p.scenes[0];
    inside.id = "scene_002";
    inside.name = "안";
    inside.objects = {box("hero", 200, 100, 32, 32)};
    SceneObject welcome = box("welcome", 350, 250, 10, 10);
    welcome.event.trigger = EventTrigger::Autorun;
    welcome.event.commands = {add("arrived", 1)};
    inside.objects.push_back(welcome);
    p.scenes.push_back(inside);
    return p;
}

} // namespace

TEST_CASE("change scene command moves the player to the arrival point and stops the event") {
    GameWorld world(twoScenes(changeScene("scene_002", std::pair{64.0, 128.0}, "left")), noScripts());
    world.enterScene("scene_001");
    world.setFacing(Facing::Right);
    step(world, confirm());
    CHECK(world.scene()->id == "scene_002");
    CHECK(world.player()->x == 64.0 && world.player()->y == 128.0);
    CHECK(world.facing() == Facing::Left);
    CHECK(intVar(world, "before") == 1);
    CHECK(intVar(world, "after") == -1); // 장면이 바뀌면 남은 명령은 실행되지 않는다
    step(world);
    CHECK(intVar(world, "arrived") == 1); // 새 장면의 자동 실행
}

TEST_CASE("change scene without a position keeps the edited spot and the current facing") {
    GameWorld world(twoScenes(changeScene("scene_002", std::nullopt, "")), noScripts());
    world.enterScene("scene_001");
    world.setFacing(Facing::Right);
    step(world, confirm());
    CHECK(world.scene()->id == "scene_002");
    CHECK(world.player()->x == 200.0 && world.player()->y == 100.0);
    CHECK(world.facing() == Facing::Right); // 방향 "그대로"
}

TEST_CASE("change scene to a missing scene fails the event and stays") {
    GameWorld world(twoScenes(changeScene("scene_404", std::pair{0.0, 0.0}, "up")), noScripts());
    world.enterScene("scene_001");
    world.setFacing(Facing::Right);
    step(world, confirm());
    CHECK(world.scene()->id == "scene_001");
    CHECK(world.failedEventCount() == 1);
    CHECK(intVar(world, "after") == -1);
}

// ---------------------------------------------------------------- 실행 조건·반복

namespace {

EventCondition when(std::string variable, CompareOp compare, Value value) {
    return {std::move(variable), compare, std::move(value)};
}

} // namespace

TEST_CASE("조건이 맞지 않는 조사·접촉 이벤트는 실행되지 않고, 조사는 다음 객체를 찾는다") {
    SceneObject locked = box("locked", 0, 32, 32, 32); // 플레이어 바로 아래
    locked.event.commands = {add("locked", 1)};
    locked.event.conditions = {when("door_open", CompareOp::Equal, true)};
    SceneObject floor = box("floor", 0, 0, 32, 32); // 밟고 있는 통과 가능한 객체
    floor.passable = true;
    floor.event.commands = {add("floor", 1)};
    SceneObject trap = box("trap", 100, 0, 20, 32);
    trap.event.trigger = EventTrigger::Touch;
    trap.event.conditions = {when("armed", CompareOp::Equal, true)};
    trap.event.commands = {add("trap", 1)};
    GameWorld world(playerProject({locked, floor, trap}), noScripts());
    world.enterScene("scene_001");
    step(world, confirm());
    CHECK(intVar(world, "locked") == -1);
    CHECK(intVar(world, "floor") == 1); // 조건이 맞지 않는 앞 객체 대신 밟고 있는 객체
    world.variables().set("door_open", true);
    step(world, confirm());
    CHECK(intVar(world, "locked") == 1);

    hold(world, InputAction::Right, 30); // 함정에 부딪힘 (조건 안 맞음)
    CHECK(intVar(world, "trap") == -1);
}

TEST_CASE("반복하지 않는 자동 실행은 조건이 처음 맞을 때 한 번 실행된다") {
    SceneObject boss = box("boss", 300, 200, 10, 10);
    boss.event.trigger = EventTrigger::Autorun;
    boss.event.conditions = {when("boss_ready", CompareOp::Equal, true)};
    boss.event.commands = {add("cutscene", 1)};
    GameWorld world(playerProject({boss}), noScripts());
    world.enterScene("scene_001");
    for (int i = 0; i < 5; ++i) {
        step(world);
    }
    CHECK(intVar(world, "cutscene") == -1);
    world.variables().set("boss_ready", true); // 다른 이벤트·스크립트가 스위치를 켠 것처럼
    for (int i = 0; i < 5; ++i) {
        step(world);
    }
    CHECK(intVar(world, "cutscene") == 1);
    world.enterScene("scene_001"); // 장면에 다시 들어오면 다시 한 번
    step(world);
    CHECK(intVar(world, "cutscene") == 2);
}

TEST_CASE("반복하는 자동 실행은 조건이 맞는 동안 한 틱에 한 번씩 다시 실행되고, 끝나면 조작이 돌아온다") {
    SceneObject counter = box("counter", 300, 200, 10, 10);
    counter.event.trigger = EventTrigger::Autorun;
    counter.event.repeat = true;
    counter.event.conditions = {when("n", CompareOp::Less, std::int64_t{5})}; // 아직 없는 n은 0으로 본다
    counter.event.commands = {add("n", 1)};
    GameWorld world(playerProject({counter}), noScripts());
    world.enterScene("scene_001");
    for (int i = 0; i < 3; ++i) {
        step(world);
    }
    CHECK(intVar(world, "n") == 3);
    hold(world, InputAction::Right, 10);
    CHECK(intVar(world, "n") == 5);
    CHECK(!world.busy());
    CHECK(world.player()->x > 0.0);
}

TEST_CASE("병렬 처리는 반복을 켜면 끝날 때마다 다시 실행되고, 끄면 한 번만 실행된다") {
    SceneObject blink = box("blink", 300, 200, 10, 10);
    blink.event.trigger = EventTrigger::Parallel;
    blink.event.repeat = true;
    blink.event.commands = {add("blink", 1), wait(0.1)};
    SceneObject once = box("once", 300, 100, 10, 10);
    once.event.trigger = EventTrigger::Parallel;
    once.event.commands = {add("once", 1)};
    SceneObject gated = box("gated", 300, 0, 10, 10);
    gated.event.trigger = EventTrigger::Parallel;
    gated.event.repeat = true;
    gated.event.conditions = {when("rain", CompareOp::Equal, true)};
    gated.event.commands = {add("drop", 1)};
    GameWorld world(playerProject({blink, once, gated}), noScripts());
    world.enterScene("scene_001");
    hold(world, InputAction::Right, 60); // 1초: 병렬 처리는 조작을 막지 않는다
    CHECK(world.player()->x > 100.0);
    CHECK(intVar(world, "blink") >= 7);
    CHECK(intVar(world, "blink") <= 11);
    CHECK(intVar(world, "once") == 1);
    CHECK(intVar(world, "drop") == -1);
    world.variables().set("rain", true);
    for (int i = 0; i < 4; ++i) {
        step(world);
    }
    CHECK(intVar(world, "drop") == 4);
    world.variables().set("rain", false);
    for (int i = 0; i < 4; ++i) {
        step(world);
    }
    CHECK(intVar(world, "drop") == 4);
}
