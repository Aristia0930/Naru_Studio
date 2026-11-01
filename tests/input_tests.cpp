#include "TestMain.h"
#include "naru/runtime/Input.h"
#include <algorithm>
using namespace naru;

TEST_CASE("입력 Python 설정의 생성·읽기와 별칭·OFF·주석이 일치한다") {
    InputSettings settings;
    settings.defaultControl = false;
    settings.playerSpeed = 321.25;
    settings.bindings["confirm"] = {"Return"};
    settings.bindings["cancel"].clear();
    settings.bindings["jump"] = {"Space"};
    auto source = inputSettingsSource(settings);
    auto parsed = parseInputSettings(source);
    CHECK(parsed.settings && *parsed.settings == settings);
    auto at = source.find("\"Return\"");
    source.replace(at, 8, "'Enter'");
    source += "# 사용자 주석\n";
    source.insert(source.find("KEY_BINDINGS = {") + 16, "\n# 키 목록 안 주석\n");
    CHECK(parseInputSettings(source).settings == parsed.settings);
    settings.playerSpeed = 500;
    auto edited = inputSettingsSource(settings, source);
    CHECK(edited.ends_with("# 사용자 주석\n"));
    CHECK(edited.find("# 키 목록 안 주석") != std::string::npos);
    CHECK(edited.find("DEFAULT_CONTROL_ENABLED = False") != std::string::npos);
    CHECK(parseInputSettings(edited).settings == std::optional<InputSettings>{settings});
}

TEST_CASE("입력 설정의 잘못된 값·실행 코드·중복·과도한 중첩은 평가하지 않고 거부한다") {
    auto base = inputSettingsSource({});
    auto replace = [&](const std::string& from, const std::string& to) {
        auto text = base;
        auto at = text.find(from);
        CHECK(at != std::string::npos);
        if (at != std::string::npos) text.replace(at, from.size(), to);
        CHECK(!parseInputSettings(text).settings);
    };
    replace("DEFAULT_CONTROL_ENABLED = True", "DEFAULT_CONTROL_ENABLED = 1");
    replace("240.0", "-1");
    replace("240.0", "100001");
    replace("240.0", "'fast'");
    replace("240.0", "float('nan')");
    replace("240.0", "2 ** 10");
    replace("\"up\"", "\"UP\"");
    replace("\"Up\"", "\"Unknown key\"");
    replace("\"Up\"", "17");
    CHECK(!parseInputSettings(base + "import os\n").settings);
    CHECK(!parseInputSettings(base + "PLAYER_SPEED = 1\n").settings);
    CHECK(!parseInputSettings(base + "def run(ctx):\n    pass\n").settings);
    CHECK(!parseInputSettings("DEFAULT_CONTROL_ENABLED = True\nPLAYER_SPEED = 240\nKEY_BINDINGS = {'up': [], 'up': []}").settings);
    CHECK(!parseInputSettings("DEFAULT_CONTROL_ENABLED = True\nPLAYER_SPEED = 240\nKEY_BINDINGS = {\n").settings);
    CHECK(!parseInputSettings("").settings);
}

TEST_CASE("단일·이중 따옴표와 끝 쉼표를 쓰는 실제 Python 리터럴 설정을 읽는다") {
    const std::string source =
        "# 변수 대입 외 코드는 쓰지 않는다\nDEFAULT_CONTROL_ENABLED = True  # ON\n"
        "PLAYER_SPEED = 60\nKEY_BINDINGS = {\n"
        " 'up': ['I',], 'down': ['K'], 'left': ['J'], 'right': ['L'],\n"
        " 'confirm': ['Enter'], 'cancel': [], 'dash': ['Shift'],\n}\n";
    auto parsed = parseInputSettings(source);
    CHECK(parsed.settings.has_value());
    if (!parsed.settings) return;
    CHECK(parsed.settings->playerSpeed == 60);
    CHECK((parsed.settings->bindings.at("confirm") == std::vector<std::string>{"Return"}));
    CHECK(parsed.settings->bindings.at("cancel").empty());
    auto duplicate = source;
    auto at = duplicate.find("['Enter']");
    duplicate.replace(at, 9, "['Enter', 'Return']");
    CHECK(!parseInputSettings(duplicate).settings);
    InputMap map;
    for (const auto& [action, keys] : parsed.settings->bindings) map.bind(action, keys);
    CHECK((map.actionsForKey("l") == std::vector<std::string>{"right"}));
    CHECK(map.actionsForKey("Right").empty());
}
TEST_MAIN()

TEST_CASE("동시 조합은 순서와 무관하게 모두 눌려야 하고 대체 키는 OR로 동작한다") {
    InputMap map;
    map.bind("special", {"e+shift", "F"});
    map.bind("plain", {"E"});
    CHECK((map.keys("special") == std::vector<std::string>{"Shift + E", "F"}));
    CHECK(!map.actionHeld("special", {}));
    CHECK(!map.actionHeld("special", {"E"}));
    CHECK(!map.actionHeld("special", {"Left Shift"}));
    CHECK(map.actionHeld("special", {"E", "Left Shift"}));
    CHECK(map.actionHeld("special", {"E", "Right Shift", "W"}));
    CHECK(map.actionHeld("plain", {"E", "Left Shift"}));
    CHECK(map.actionHeld("special", {"F"}));
    InputState input;
    input.set("special", map.actionHeld("special", {"E"}));
    CHECK(!input.wasPressed("special"));
    input.set("special", map.actionHeld("special", {"E", "Left Shift"}));
    CHECK(input.wasPressed("special"));
    input.endFrame();
    input.set("special", map.actionHeld("special", {"E", "Left Shift"}));
    CHECK(input.isHeld("special") && !input.wasPressed("special"));
    input.set("special", map.actionHeld("special", {"Left Shift"}));
    CHECK(!input.isHeld("special"));
    input.set("special", map.actionHeld("special", {"E", "Left Shift"}));
    CHECK(input.wasPressed("special"));
}

TEST_CASE("조합 별칭·플러스 키·중복/범위 겹침·4키 한도를 검증한다") {
    CHECK(InputMap::canonicalBinding(" E + ctrl + SHIFT ") == std::optional<std::string>{"Ctrl + Shift + E"});
    CHECK(InputMap::canonicalBinding("Keypad +") == std::optional<std::string>{"Keypad +"});
    CHECK(InputMap::canonicalBinding("Keypad ++ctrl") == std::optional<std::string>{"Ctrl + Keypad +"});
    CHECK(InputMap::canonicalBinding("ctrl + a + alt + shift").has_value());
    for (const auto& bad : {"", "Shift+", "+E", "Shift++E", "E+E", "Enter+Return", "Shift+Left Shift",
                            "Ctrl+Right Ctrl", "A+B+C+D+E", "Wrong+E"})
        CHECK(!InputMap::canonicalBinding(bad));
    InputMap map;
    map.bind("plus", {"Ctrl + Keypad +"});
    CHECK(map.actionHeld("plus", {"Right Ctrl", "Keypad +"}));
    CHECK(!map.actionHeld("plus", {"Keypad +"}));
    auto settings = InputSettings{};
    settings.bindings["special"] = {"E + Shift", "F"};
    auto parsed = parseInputSettings(inputSettingsSource(settings));
    CHECK(parsed.settings && parsed.settings->bindings.at("special")[0] == "Shift + E");
    settings.bindings["special"] = {"Shift+E", "E+Shift"};
    CHECK(!parseInputSettings(inputSettingsSource(settings)).settings);
}
