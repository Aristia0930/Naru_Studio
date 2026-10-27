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

    python::ScriptResult bad;
    CHECK(g_runtime->callModuleFunction(*globals.loadedPath("objects.py"), "bad_color", f_ctxOf(world), &bad) ==
          python::CallStatus::Failed);
    CHECK(bad.error.type == "ValueError");
}

TEST_CASE("스크립트에 공개된 모든 이름과 훅이 docs/SCRIPT_API.md에 적혀 있다") {    // 새 ctx 값·함수나 훅을 추가하면 이 테스트가 문서 갱신을 요구한다.
    std::ifstream in(std::filesystem::path(reinterpret_cast<const char8_t*>(NARU_SCRIPT_API_DOC)), std::ios::binary);
    std::string doc((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!doc.empty());
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
