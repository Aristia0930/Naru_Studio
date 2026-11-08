#include "TestMain.h"

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/data/Database.h"
#include "naru/events/BuiltinCommands.h"
#include "naru/python/PythonGameScripts.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/GameWorld.h"
#include "naru/runtime/Session.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

TEST_CASE("Python 소리 API의 호출 인자·F5 보호·오류 파일과 traceback") {
    struct Host : AudioHost {
        int calls=0; bool fail=false;
        std::string playBgm(const std::string& id,double v,bool loop) override {
            ++calls; if (fail) return "소리 장치 검사 오류";
            CHECK(id=="music" && v==40 && loop); return {};
        }
        std::string playSe(const std::string& id,double v) override { ++calls; CHECK(id=="music" && v==60); return {}; }
        std::string stopBgm(double fade) override { ++calls; CHECK(fade==1.5); return {}; }
        void stopSe() override { ++calls; }
    } host;
    Fixture f;
    CHECK(g_runtime->runFile(script("audio_api.py"), f.ctx).ok);
    Project p;
    p.assets = {{"music","음악","audio","assets/music.mp3","music.mp3",std::string(64,'0'),""},
                {"picture","그림","image","assets/picture.png","picture.png",std::string(64,'1'),""}};
    GameWorld world(p,{}); world.setAudio(&host);
    CommandContext ctx{world.variables(),world.logger(),nullptr,std::nullopt,nullptr,&world};
    Value returned;
    auto result=g_runtime->runFile(script("audio_api.py"),ctx,"game",&returned);
    CHECK(result.ok); CHECK(host.calls==4); CHECK(returned==Value{std::int64_t{4}});
    host.fail=true;
    result=g_runtime->runFile(script("audio_api.py"),ctx,"failure");
    CHECK(!result.ok); CHECK(result.error.type=="RuntimeError");
    CHECK(result.error.file.find("audio_api.py")!=std::string::npos);
    CHECK(result.error.line>0 && !result.error.traceback.empty());
}

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

TEST_CASE("스크립트에서 데이터베이스를 읽는다 (ctx.db)") {
    Database db;
    DataType items = makeTemplate(DataTemplate::Item, db);
    items.records = {{"potion", "포션", {{"price", std::int64_t{50}}}}, {"ether", "에테르", {}}};
    db.types.push_back(items);
    DataType monsters = makeTemplate(DataTemplate::Monster, db);
    monsters.records = {{"slime", "슬라임", {{"drop", std::string("potion")}}}};
    db.types.push_back(monsters);

    Fixture f;
    f.ctx.database = &db;
    auto result = g_runtime->runFile(script("db_access.py"), f.ctx);
    // 마지막 줄(10행)은 없는 종류를 읽어 KeyError가 나야 한다
    if (result.error.line != 10) {
        std::printf("  실제: %s\n", result.summary().c_str());
    }
    CHECK(!result.ok && result.error.type == "KeyError" && result.error.line == 10);
    CHECK(f.variables.get("slime_name") == Value{std::string("슬라임")});
    CHECK(f.variables.get("slime_hp") == Value{std::int64_t{30}}); // 템플릿 기본값
    CHECK(f.variables.get("drop_price") == Value{std::int64_t{50}});
    CHECK(f.variables.get("missing_is_none") == Value{true});
    CHECK(f.variables.get("item_count") == Value{std::int64_t{2}});
    CHECK(f.variables.get("types") == Value{std::string("item,monster")});

    Fixture noDb;
    CHECK(!g_runtime->runFile(script("db_access.py"), noDb.ctx).ok);
}

TEST_CASE("async 함수는 아직 지원하지 않는다고 명확히 보고한다") {
    Fixture f;
    auto result = g_runtime->runFile(script("async_run.py"), f.ctx);
    CHECK(!result.ok);
    CHECK(result.error.type == "NotImplementedError");
}

TEST_CASE("반환값을 받는다 (None·bool·int·float·str만)") {
    Fixture f;
    Value returned;
    CHECK(g_runtime->runFile(script("returns.py"), f.ctx, "number", &returned).ok);
    CHECK(returned == Value{std::int64_t{42}});
    auto bad = g_runtime->runFile(script("returns.py"), f.ctx, "bad", &returned);
    CHECK(!bad.ok);
    CHECK(bad.error.type == "TypeError");
}

TEST_CASE("게임 밖에서는 ctx.player·ctx.input·ctx.scene이 None이다") {
    Fixture f;
    Value returned;
    CHECK(g_runtime->runFile(script("outside_game.py"), f.ctx, "run", &returned).ok);
    CHECK(returned == Value{true});
}

// ---------------------------------------------------------------- 전역 스크립트

namespace {

// 플레이어가 있는 장면과, 전역 스크립트 함수를 부르는 자동 실행 이벤트.
Project globalProject() {
    Project p;
    p.name = "global";
    Scene s;
    s.id = "scene_001";
    s.name = "마을";
    SceneObject hero;
    hero.id = "hero";
    hero.name = "주인공";
    s.objects.push_back(hero);
    s.playerObjectId = "hero";
    SceneObject caller;
    caller.id = "caller";
    caller.name = "호출";
    caller.x = 500;
    caller.event.trigger = EventTrigger::Autorun;
    EventCommand bump;
    bump.type = EventCommandType::CallScript;
    bump.script = "global_a.py";
    bump.function = "bump";
    bump.variable = "after_bump";
    EventCommand read = bump;
    read.script = "global_b.py";
    read.function = "read_a";
    read.variable = "read_from_b";
    caller.event.commands = {bump, read};
    s.objects.push_back(caller);
    p.scenes.push_back(s);
    p.startSceneId = s.id;
    return p;
}

// player와 같은 규칙: 전역 스크립트로 읽어 둔 파일은 실행 중인 모듈의 함수를 부른다.
ScriptRunner globalAwareRunner(python::PythonGameScripts& globals) {
    return [&globals](const std::string& name, const std::string& function, CommandContext& ctx) {
        ScriptCallResult call;
        if (auto loaded = globals.loadedPath(name)) {
            if (g_runtime->callModuleFunction(*loaded, function, ctx, nullptr, &call.returned) != python::CallStatus::Ok) {
                call.error = "실패";
            }
            return call;
        }
        if (!g_runtime->runFile(script(name.c_str()), ctx, function, &call.returned).ok) {
            call.error = "실패";
        }
        return call;
    };
}

std::optional<Value> var(GameWorld& world, const char* name) {
    return world.variables().get(name);
}

// 테스트에서 게임 상태로 모듈 함수를 직접 부를 때 쓰는 컨텍스트 (world보다 짧게 산다).
CommandContext& f_ctxOf(GameWorld& world) {
    static std::unique_ptr<CommandContext> ctx;
    ctx = std::make_unique<CommandContext>(
        CommandContext{world.variables(), world.logger(), &world, std::nullopt, &world.project().database, &world});
    return *ctx;
}
} // namespace

TEST_CASE("전역 스크립트: 훅·모듈 전역 변수 유지·이벤트에서 모듈 함수 호출·import") {
    python::PythonGameScripts globals(*g_runtime, {script("global_a.py"), script("global_b.py")});
    GameWorld world(globalProject(), globalAwareRunner(globals));
    world.setGameScripts(&globals);
    CHECK(world.startGame("scene_001"));
    CHECK(var(world, "started") == Value{true});
    CHECK(var(world, "entered") == Value{std::string("마을")});

    InputState input;
    world.update(1.0 / 60.0, input); // on_update(counter=1) → 자동 실행: bump(+100), read_a
    CHECK(var(world, "after_bump") == Value{std::int64_t{101}});
    CHECK(var(world, "read_from_b") == Value{std::int64_t{101}});
    world.update(1.0 / 60.0, input);
    world.update(1.0 / 60.0, input);
    CHECK(var(world, "ticks") == Value{std::int64_t{103}}); // 모듈의 counter가 이어진다
    CHECK(world.failedEventCount() == 0);
}

TEST_CASE("전역 스크립트가 만든 동작(dash)과 키 배치로 플레이어를 바꾼다") {
    python::PythonGameScripts globals(*g_runtime, {script("global_a.py")});
    GameWorld world(globalProject(), globalAwareRunner(globals));
    world.setGameScripts(&globals);
    world.startGame("scene_001");
    CHECK((world.inputMap().keys("dash") == std::vector<std::string>{"Shift"}));
    CHECK(InputMap::keyMatches("Shift", "Left Shift"));
    InputState input;
    input.press("dash");
    world.update(1.0 / 60.0, input);
    CHECK(world.playerSpeed() == 500.0);
}

TEST_CASE("전역 스크립트 변수 이름이 겹치면 두 위치를 알려 준다") {
    auto errors = g_runtime->checkGlobalScripts({script("global_a.py"), script("global_conflict.py")});
    CHECK(errors.size() == 1);
    if (!errors.empty()) {
        CHECK(errors[0].type == "NameConflict");
        CHECK(errors[0].message.find("counter") != std::string::npos);
        CHECK(errors[0].message.find("global_a.py:2") != std::string::npos);
        CHECK(errors[0].file == script("global_conflict.py"));
        CHECK(errors[0].line == 3);
    }
    CHECK(g_runtime->checkGlobalScripts({script("global_a.py"), script("global_b.py")}).empty());
    // 표준 모듈과 같은 파일 이름
    auto std_errors = g_runtime->checkGlobalScripts({script("random.py")});
    bool stdConflict = false;
    for (const auto& e : std_errors) {
        stdConflict = stdConflict || e.message.find("표준 모듈") != std::string::npos;
    }
    CHECK(stdConflict);
}

TEST_CASE("오류가 난 전역 스크립트 훅은 한 번만 보고하고 다시 부르지 않는다") {
    python::PythonGameScripts globals(*g_runtime, {script("global_error.py"), script("global_a.py")});
    GameWorld world(globalProject(), globalAwareRunner(globals));
    world.setGameScripts(&globals);
    world.startGame("scene_001");
    InputState input;
    for (int i = 0; i < 5; ++i) {
        world.update(1.0 / 60.0, input);
    }
    int valueErrors = 0;
    for (const auto& e : world.logger().entries()) {
        valueErrors += e.message.find("ValueError") != std::string::npos ? 1 : 0;
    }
    CHECK(valueErrors == 1);
    CHECK(world.failedEventCount() == 1);
    // 다른 전역 스크립트의 on_update는 계속 불린다 (bump +100 포함)
    CHECK(var(world, "ticks") == Value{std::int64_t{105}});
}

TEST_CASE("스크립트로 다른 객체를 찾고 바꾸고 움직인다 (ctx.scene.object/find/objects)") {
    Project p = globalProject();
    SceneObject box;
    box.id = "box";
    box.name = "상자";
    box.x = 100;
    p.scenes[0].objects.push_back(box);
    p.scenes[0].objects[1].event.commands.clear(); // 자동 실행 호출 이벤트는 쓰지 않는다
    python::PythonGameScripts globals(*g_runtime, {script("objects.py")});
    GameWorld world(p, globalAwareRunner(globals));
    world.setGameScripts(&globals);
    world.startGame("scene_001");
    InputState input;
    world.update(1.0 / 60.0, input);
    world.update(1.0 / 60.0, input);
    CHECK(world.object("box")->x == 108.0); // on_update에서 4px씩 두 번

    Value returned;
    CHECK(g_runtime->callModuleFunction(*globals.loadedPath("objects.py"), "inspect", f_ctxOf(world), nullptr,
                                        &returned) == python::CallStatus::Ok);
    CHECK(returned == Value{10.0});
    CHECK(var(world, "box_id") == Value{std::string("box")});
    CHECK(var(world, "hero_is_player") == Value{true});
    CHECK(var(world, "count") == Value{std::int64_t{3}});
    CHECK(var(world, "missing") == Value{true});
    CHECK(world.object("box")->color == "#ff8800");
    CHECK(world.object("box")->passable);
    CHECK(!world.object("box")->visible);
    CHECK(var(world, "box_visible") == Value{false});

    python::ScriptResult bad;
    CHECK(g_runtime->callModuleFunction(*globals.loadedPath("objects.py"), "bad_color", f_ctxOf(world), &bad) ==
          python::CallStatus::Failed);
    CHECK(bad.error.type == "ValueError");
}

TEST_CASE("ctx.scene.change는 도착 위치와 방향을 받는다") {
    Project p = globalProject();
    p.scenes[0].objects[1].event.commands.clear();
    Scene second = p.scenes[0];
    second.id = "scene_002";
    p.scenes.push_back(second);
    GameWorld world(p, ScriptRunner{});
    world.startGame("scene_001");
    for (const char* bad : {"half", "bad_facing"}) {
        auto result = g_runtime->runFile(script("scene_change.py"), f_ctxOf(world), bad);
        CHECK(!result.ok && result.error.type == "ValueError");
    }
    CHECK(!world.sceneChangePending());
    CHECK(g_runtime->runFile(script("scene_change.py"), f_ctxOf(world), "go").ok);
    InputState input;
    world.update(1.0 / 60.0, input);
    CHECK(world.scene()->id == "scene_002");
    CHECK(world.player()->x == 64.0 && world.player()->y == 96.0);
    CHECK(world.facing() == Facing::Left);
}

TEST_CASE("Python 이미지 변경은 실행 사본만 바꾸고 잘못된 참조는 기존 값을 보존한다") {
    auto p = globalProject();
    p.scenes[0].objects[1].event.commands.clear();
    p.assets = {{"picture", "picture", "image", "assets/p.png", "p.png", std::string(64, 'a')},
                {"sound", "sound", "audio", "assets/s.wav", "s.wav", std::string(64, 'b')}};
    GameWorld world(p, ScriptRunner{}); CHECK(world.startGame("scene_001"));
    CHECK(g_runtime->runFile(script("images.py"), f_ctxOf(world)).ok);
    auto bad = g_runtime->runFile(script("images.py"), f_ctxOf(world), "bad_type");
    CHECK(!bad.ok && bad.error.type == "TypeError" && bad.error.line > 0);
    CHECK(world.project() == p);
}

TEST_CASE("전체 가이드의 이벤트 함수는 실제 엔진 상태·반환·F5 보호·DB·오류를 검증한다") {
    auto p = globalProject();
    auto& first = p.scenes[0];
    for (auto& obj : first.objects) obj.event.commands.clear();
    first.objects[0].x = 64; first.objects[0].y = 64; first.objects[0].width = 16; first.objects[0].height = 16;
    first.objects[1].id = "tree"; first.objects[1].name = "나무"; first.objects[1].x = 200; first.objects[1].y = 200;
    first.backgroundImage.asset = "day";
    for (const auto* id : {"day", "night", "tree", "hero"})
        p.assets.push_back({id, id, "image", std::string("assets/") + id + ".png", std::string(id) + ".png", std::string(64, 'a')});
    auto second = first; second.id = "scene_002";
    for (auto& obj : second.objects) obj.id += "_2";
    second.playerObjectId += "_2"; p.scenes.push_back(second);
    auto items = makeTemplate(DataTemplate::Item, p.database);
    items.records = {{"potion", "포션", {{"price", std::int64_t{50}}}}}; p.database.types.push_back(items);
    GameWorld world(p, ScriptRunner{}); CHECK(world.startGame("scene_001"));
    const auto file = std::string(NARU_EXAMPLES_DIR) + "/python_guide.py";
    Value returned;
    const auto call = [&](const char* function) {
        auto result = g_runtime->runFile(file, f_ctxOf(world), function, &returned);
        if (!result.ok) std::printf("%s: %s\n", function, result.summary().c_str());
        CHECK(result.ok);
    };
    call("run"); call("variables_demo"); call("variables_demo");
    CHECK(var(world, "guide_count") == Value{std::int64_t{2}} && var(world, "guide_switch") == Value{false});
    call("inspect_database"); call("read_item_price");
    CHECK(returned == Value{std::int64_t{50}} && var(world, "guide_item_name") == Value{std::string("포션")});
    call("set_night"); CHECK(world.scene()->backgroundImage.asset == "night");
    call("set_day"); call("toggle_background"); CHECK(world.scene()->backgroundImage.asset == "night");
    call("toggle_background"); CHECK(world.scene()->backgroundImage.asset == "day");
    call("clear_background"); CHECK(world.scene()->backgroundImage.asset.empty());
    call("change_object_image"); CHECK(world.object("tree")->image.asset == "tree");
    call("clear_object_image"); CHECK(world.object("tree")->image.asset.empty());
    call("change_player_image"); CHECK(world.player()->image.asset == "hero");
    call("hide_object"); CHECK(!world.object("tree")->visible);
    call("show_object"); CHECK(world.object("tree")->visible);
    call("toggle_passable"); CHECK(world.object("tree")->passable);
    call("toggle_passable"); CHECK(!world.object("tree")->passable);
    call("change_object_color"); CHECK(world.object("tree")->color == "#ff8800");
    call("move_object"); CHECK(world.object("tree")->x == 248);
    call("teleport_object"); CHECK(world.object("tree")->x == 128 && world.object("tree")->y == 128);
    call("inspect_scene"); call("inspect_player");
    call("move_player"); CHECK(world.player()->x == 112);
    call("teleport_player"); CHECK(world.player()->x == 64 && world.player()->y == 64);
    call("face_right"); CHECK(world.facing() == Facing::Right);
    call("set_player_speed"); CHECK(world.playerSpeed() == 360);
    call("disable_default_control"); CHECK(!world.defaultControl());
    call("enable_default_control"); CHECK(world.defaultControl());
    call("interact"); CHECK(returned == Value{false});
    call("inspect_input"); call("bind_runtime_keys");
    CHECK(world.inputMap().keys("guide_special").size() == 2);
    call("disable_runtime_action"); CHECK(world.inputMap().keys("guide_special").empty());
    call("log_examples");
    auto error = g_runtime->runFile(file, f_ctxOf(world), "error_example");
    CHECK(!error.ok && error.error.type == "ValueError" && error.error.line > 0 && !error.error.traceback.empty());
    call("change_scene"); CHECK(world.sceneChangePending()); world.update(1.0 / 60.0, {});
    CHECK(world.scene()->id == "scene_002" && world.player()->x == 64 && world.player()->y == 96 && world.facing() == Facing::Up);
    CHECK(world.project() == p);
    Fixture outside; outside.ctx.database = &p.database;
    CHECK(g_runtime->runFile(std::string(NARU_EXAMPLES_DIR) + "/guide_global.py", outside.ctx).ok);
    for (const auto* function : {"run", "variables_demo", "inspect_database", "read_item_price", "set_day", "set_night", "toggle_background", "clear_background",
        "change_object_image", "clear_object_image", "change_player_image", "hide_object", "show_object", "toggle_passable", "change_object_color", "move_object", "teleport_object",
        "inspect_scene", "inspect_player", "move_player", "teleport_player", "face_right", "set_player_speed", "disable_default_control", "enable_default_control", "interact",
        "inspect_input", "bind_runtime_keys", "disable_runtime_action", "queue_common_event", "change_scene", "log_examples"}) {
        CHECK(g_runtime->runFile(file, outside.ctx, function).ok);
    }
}

TEST_CASE("가이드의 공통 이벤트는 F6 예약·대기·반환값·호출 뒤 명령을 연결한다") {
    Project p;
    EventCommand wait; wait.type = EventCommandType::Wait; wait.seconds = .1;
    EventCommand done; done.type = EventCommandType::SetVariable; done.variable = "guide_done"; done.value = true;
    p.commonEvents = {{"recover", "회복", {EventTrigger::Action, {}, false, {wait, done}}}};
    EventCommand call; call.type = EventCommandType::CallScript; call.script = "python_guide.py"; call.function = "queue_common_event"; call.variable = "guide_queued";
    EventCommand after = done; after.variable = "guide_after";
    const auto file = std::string(NARU_EXAMPLES_DIR) + "/python_guide.py";
    ScriptRunner runner = [&](const std::string&, const std::string& function, CommandContext& ctx) {
        ScriptCallResult result; auto scriptResult = g_runtime->runFile(file, ctx, function, &result.returned);
        if (!scriptResult.ok) result.error = scriptResult.summary(); return result;
    };
    Session session({}, .05); session.setProject(&p); session.start(compileEvent({call, after}, runner));
    CHECK(session.variables().get("guide_queued") == Value{true});
    CHECK(!session.variables().get("guide_done") && !session.variables().get("guide_after"));
    session.advance(.2); CHECK(session.state() == SessionState::Completed);
    CHECK(session.variables().get("guide_done") == Value{true} && session.variables().get("guide_after") == Value{true});
}

TEST_CASE("가이드의 전역 등록 순서·import·새 입력/유지·busy·네 훅이 실제 동작한다") {
    auto p = globalProject();
    for (auto& s : p.scenes) for (auto& o : s.objects) o.event.commands.clear();
    p.scenes[0].objects[0].x = 100; p.scenes[0].objects[0].y = 200;
    p.scenes[0].backgroundImage.asset = "day";
    for (const auto* id : {"day", "night"}) p.assets.push_back({id, id, "image", std::string("assets/") + id + ".png", "p.png", std::string(64, 'a')});
    auto second = p.scenes[0]; second.id = "scene_002";
    for (auto& o : second.objects) o.id += "_2";
    second.playerObjectId += "_2"; p.scenes.push_back(second);
    p.inputSettings.bindings["guide_toggle_background"] = {"G"}; p.inputSettings.bindings["guide_move_up"] = {"8"};
    const auto guide = std::string(NARU_EXAMPLES_DIR) + "/python_guide.py";
    const auto global = std::string(NARU_EXAMPLES_DIR) + "/guide_global.py";
    python::PythonGameScripts globals(*g_runtime, {guide, global});
    GameWorld world(p, {}); world.setGameScripts(&globals); CHECK(world.startGame("scene_001"));
    CHECK(world.failedEventCount() == 0 && var(world, "guide_scene_visits") == Value{std::int64_t{1}});
    InputState input; input.press("guide_toggle_background"); world.update(1.0 / 60.0, input);
    CHECK(world.scene()->backgroundImage.asset == "night"); input.endFrame(); world.update(1.0 / 60.0, input);
    CHECK(world.scene()->backgroundImage.asset == "night");
    input.release("guide_toggle_background"); world.update(1.0 / 60.0, input); input.endFrame();
    input.press("guide_toggle_background"); world.update(1.0 / 60.0, input); CHECK(world.scene()->backgroundImage.asset == "day"); input.endFrame();
    input.press("guide_move_up"); world.update(1.0 / 60.0, input); CHECK(world.player()->y == 152 && world.facing() == Facing::Right); input.endFrame();
    world.showMessage("", "대사 중"); input.release("guide_toggle_background"); world.update(1.0 / 60.0, input); input.endFrame();
    input.press("guide_toggle_background"); world.update(1.0 / 60.0, input); CHECK(world.scene()->backgroundImage.asset == "day");
    CHECK(world.enterScene("scene_002")); CHECK(var(world, "guide_scene_visits") == Value{std::int64_t{2}});
    CHECK(world.failedEventCount() == 0 && world.project() == p);
}

TEST_CASE("docs/examples의 예제는 문법 오류가 없고 함께 전역 등록해도 이름이 겹치지 않는다") {
    std::vector<std::string> paths;
    for (const char* name : {"chase.py", "patrol.py", "push.py", "door.py", "key_combinations.py", "key_move_face.py", "reward.py", "images/scripts/appearance.py", "python_guide.py", "guide_global.py"}) {
        std::string path = std::string(NARU_EXAMPLES_DIR) + "/" + name;
        auto syntax = g_runtime->checkSyntax(path);
        if (!syntax.ok) {
            std::printf("  %s\n", syntax.summary().c_str());
        }
        CHECK(syntax.ok);
        paths.push_back(path);
    }
    auto errors = g_runtime->checkGlobalScripts(paths);
    for (const auto& e : errors) {
        std::printf("  %s:%d %s\n", e.file.c_str(), e.line, e.message.c_str());
    }
    CHECK(errors.empty());
}

TEST_CASE("스크립트에 공개된 모든 이름과 훅이 docs/SCRIPT_API.md에 적혀 있다") {    // 새 ctx 값·함수나 훅을 추가하면 이 테스트가 문서 갱신을 요구한다.
    std::ifstream in(std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_SCRIPT_API_DOC)), std::ios::binary);
    std::string doc((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!doc.empty());
    std::ifstream cookbookFile(std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_EXAMPLES_DIR)) / "../guide/PYTHON_COOKBOOK.md", std::ios::binary);
    const std::string cookbook((std::istreambuf_iterator<char>(cookbookFile)), std::istreambuf_iterator<char>());
    CHECK(!cookbook.empty());
    Fixture f;
    Value returned;
    CHECK(g_runtime->runFile(script("api_names.py"), f.ctx, "run", &returned).ok);
    std::string names = std::holds_alternative<std::string>(returned) ? std::get<std::string>(returned) : "";
    CHECK(!names.empty());
    std::vector<std::string> all;
    for (std::size_t start = 0; start < names.size();) {
        std::size_t comma = names.find(',', start);
        if (comma == std::string::npos) {
            comma = names.size();
        }
        all.push_back(names.substr(start, comma - start));
        start = comma + 1;
    }
    for (const char* hook : {kHookGameStart, kHookSceneEnter, kHookSceneExit, kHookUpdate}) {
        all.emplace_back(hook);
    }
    for (const std::string& name : all) {
        bool documented = doc.find("`" + name) != std::string::npos || doc.find("." + name) != std::string::npos;
        if (!documented) {
            std::printf("  docs/SCRIPT_API.md에 없는 이름: %s\n", name.c_str());
        }
        CHECK(documented);
        CHECK(cookbook.find(name) != std::string::npos);
    }
    std::printf("  문서에서 확인한 이름 %zu개\n", all.size());
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

TEST_CASE("Python bind도 같은 동시 키 조합을 사용하고 잘못된 조합이면 기존 설정을 보존한다") {
    GameWorld world(globalProject(), {});
    CHECK(world.startGame("scene_001"));
    auto& ctx = f_ctxOf(world);
    CHECK(g_runtime->runFile(script("input_combinations.py"), ctx).ok);
    CHECK(world.inputMap().actionHeld("special", {"Right Shift", "E"}));
    CHECK(!world.inputMap().actionHeld("special", {"E"}));
    auto before = world.inputMap().keys("special");
    auto bad = g_runtime->runFile(script("input_combinations.py"), ctx, "invalid");
    CHECK(!bad.ok && bad.error.type == "ValueError");
    CHECK(world.inputMap().keys("special") == before);
}

TEST_CASE("문서의 보상 스크립트 예제는 이벤트 변수와 반환값을 사용해 한 번만 지급한다") {
    auto file = std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_EXAMPLES_DIR)) / "reward.py";
    auto utf8 = file.u8string();
    Fixture f;
    Value returned;
    CHECK(g_runtime->runFile(std::string(utf8.begin(), utf8.end()), f.ctx, "give", &returned).ok);
    CHECK(returned == Value{true});
    CHECK(f.variables.get("gold") == Value{std::int64_t{100}});
    CHECK(g_runtime->runFile(std::string(utf8.begin(), utf8.end()), f.ctx, "give", &returned).ok);
    CHECK(returned == Value{false});
    CHECK(f.variables.get("gold") == Value{std::int64_t{100}});
}

TEST_CASE("8 키 전역 예제는 새 입력 때 위로 이동한 후 오른쪽을 보며 유지 입력은 반복하지 않는다") {
    auto file = std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_EXAMPLES_DIR)) / "key_move_face.py";
    auto utf8 = file.u8string();
    python::PythonGameScripts globals(*g_runtime, {std::string(utf8.begin(), utf8.end())});
    auto p = globalProject();
    for (auto& scene : p.scenes) for (auto& object : scene.objects) object.event.commands.clear();
    p.scenes[0].objects[0].x = 200;
    p.scenes[0].objects[0].y = 200;
    p.inputSettings.bindings["step_up_face_right"] = {"8", "Keypad 8"};
    GameWorld world(p, globalAwareRunner(globals));
    world.setGameScripts(&globals);
    CHECK(world.startGame("scene_001"));
    InputState input;
    input.press("step_up_face_right");
    world.update(1.0 / 60.0, input);
    CHECK(world.player()->x == 200 && world.player()->y == 152);
    CHECK(world.facing() == Facing::Right);
    input.endFrame();
    world.update(1.0 / 60.0, input);
    CHECK(world.player()->y == 152);
    input.release("step_up_face_right");
    input.press("step_up_face_right");
    world.update(1.0 / 60.0, input);
    CHECK(world.player()->y == 104 && world.facing() == Facing::Right);
    CHECK(world.failedEventCount() == 0);
}


TEST_CASE("ctx.run_event는 F6에서 함수 반환 뒤 실행하고 호출 다음 명령으로 돌아온다") {
    Project project;
    EventCommand wait;
    wait.type = EventCommandType::Wait;
    wait.seconds = 0.25;
    EventCommand hp;
    hp.type = EventCommandType::SetVariable;
    hp.variable = "hp";
    hp.value = std::int64_t{100};
    project.commonEvents = {{"recover", "회복", {EventTrigger::Action, {}, false, {wait, hp}}}};
    EventCommand scriptCall;
    scriptCall.type = EventCommandType::CallScript;
    scriptCall.script = "common_events.py";
    scriptCall.variable = "returned";
    EventCommand after = hp;
    after.variable = "after";
    ScriptRunner scripts = [](const std::string& file, const std::string& function, CommandContext& ctx) {
        ScriptCallResult call;
        auto result = g_runtime->runFile(script(file.c_str()), ctx, function, &call.returned);
        if (!result.ok) call.error = result.summary();
        return call;
    };
    Session session({}, 0.25);
    session.setProject(&project);
    session.start(compileEvent({scriptCall, after}, scripts));
    CHECK(session.state() == SessionState::Running);
    CHECK(session.variables().get("script_finished") == Value{true});
    CHECK(session.variables().get("returned") == Value{std::int64_t{7}});
    CHECK(!session.variables().get("hp") && !session.variables().get("after"));
    session.advance(0.5);
    CHECK(session.state() == SessionState::Completed);
    CHECK(session.variables().get("hp") == Value{std::int64_t{100}});
    CHECK(session.variables().get("after") == Value{std::int64_t{100}});
    scriptCall.function = "fails";
    Session failed({});
    failed.setProject(&project);
    failed.start(compileEvent({scriptCall}, scripts));
    CHECK(failed.state() == SessionState::Failed);
    CHECK(!failed.variables().get("hp"));
    Fixture missing;
    missing.ctx.project = &project;
    auto result = g_runtime->runFile(script("common_events.py"), missing.ctx, "missing");
    CHECK(!result.ok && result.error.type == "KeyError");
    CHECK(result.error.line > 0 && !result.error.traceback.empty());
    Fixture outside;
    CHECK(!g_runtime->runFile(script("common_events.py"), outside.ctx).ok);
    scriptCall.function = "recursive";
    project.commonEvents.push_back({"recursive", "순환", {EventTrigger::Action, {}, false, {scriptCall}}});
    Session cycle({});
    cycle.setProject(&project);
    cycle.start(compileEvent({scriptCall}, scripts));
    CHECK(cycle.state() == SessionState::Failed);
    CHECK(cycle.runner().failureMessage().find("순환") != std::string::npos);
}

TEST_CASE("전역 훅의 ctx.run_event 예약은 첫 장면과 장면 전환 후 실행된다") {
    auto p = globalProject();
    EventCommand add;
    add.type = EventCommandType::SetVariable;
    add.variable = "recoveries";
    add.op = VariableOp::Add;
    add.value = std::int64_t{1};
    p.commonEvents = {{"recover", "회복", {EventTrigger::Action, {}, false, {add}}}};
    Scene second;
    second.id = "scene_002";
    second.name = "다음";
    p.scenes.push_back(second);
    for (auto& scene : p.scenes) for (auto& obj : scene.objects) obj.event.commands.clear();
    python::PythonGameScripts globals(*g_runtime, {script("common_events.py")});
    GameWorld world(p, globalAwareRunner(globals));
    world.setGameScripts(&globals);
    CHECK(world.startGame("scene_001"));
    world.update(1.0 / 60.0, {});
    CHECK(world.variables().get("recoveries") == Value{std::int64_t{1}});
    CHECK(world.enterScene("scene_002"));
    world.update(1.0 / 60.0, {});
    CHECK(world.variables().get("recoveries") == Value{std::int64_t{2}});
}


TEST_CASE("Python 이름표 읽기·변경·오류·장면 재진입 초기화") {
    Fixture preview; CHECK(g_runtime->runFile(script("nameplates.py"),preview.ctx).ok);
    auto p=globalProject(); p.scenes[0].objects[0].id="npc";
    p.scenes[0].playerObjectId="npc";
    GameWorld world(p,{}); CHECK(world.startGame(p.startSceneId));
    auto& ctx=f_ctxOf(world); Value returned;
    auto result=g_runtime->runFile(script("nameplates.py"),ctx,"change",&returned);
    CHECK(result.ok); CHECK(returned==Value{true});
    CHECK(world.object("npc")->showName && world.object("npc")->nameText.empty());
    CHECK(!world.project().scenes[0].objects[0].showName);
    CHECK(world.enterScene(p.startSceneId)); CHECK(!world.object("npc")->showName);
}
