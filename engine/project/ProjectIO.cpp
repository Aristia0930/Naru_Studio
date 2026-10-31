#include "naru/project/ProjectIO.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <system_error>

namespace naru {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr const char* kSampleScript =
    "# 이벤트나 실행 메뉴에서 호출하는 게임 스크립트 예제\n"
    "def run(ctx):\n"
    "    count = ctx.variables.get(\"visits\", 0) + 1\n"
    "    ctx.variables.set(\"visits\", count)\n"
    "    ctx.log(f\"방문 횟수: {count}\")\n";

// 새 프로젝트의 전역 스크립트 예제 (프로젝트 설정 → 전역 스크립트에 등록된 상태로 만든다).
constexpr const char* kSampleGlobalScript =
    "# 전역 스크립트: 게임 전체에서 실행된다 (프로젝트 설정 → 전역 스크립트에서 등록·순서 변경).\n"
    "# 아래 이름의 함수가 있으면 게임이 부른다. 필요 없는 함수는 지워도 된다.\n"
    "#   on_game_start(ctx)  게임 시작   on_scene_enter(ctx) / on_scene_exit(ctx)  장면 들어옴/나감\n"
    "#   on_update(ctx)      매 프레임 (1/60초)\n"
    "# 이벤트의 \"스크립트 호출\"로 이 파일의 함수를 부르면 실행 중인 이 모듈에서 실행되어 아래 전역 변수를 함께 쓴다.\n"
    "# 다른 스크립트에서는 import game 으로 쓸 수 있다.\n"
    "\n"
    "WALK_SPEED = 240\n"
    "DASH_SPEED = 420\n"
    "combo = 0  # 게임 실행 중 유지되는 파이썬 전역 변수\n"
    "\n"
    "\n"
    "def on_game_start(ctx):\n"
    "    # 새 동작 \"dash\"를 Shift 키에 연결한다 (키 배치도 이렇게 바꿀 수 있다)\n"
    "    ctx.input.bind(\"dash\", [\"Shift\"])\n"
    "    ctx.log(\"게임 시작 (전역 스크립트)\")\n"
    "\n"
    "\n"
    "def on_scene_enter(ctx):\n"
    "    ctx.log(f\"장면 진입: {ctx.scene.name}\")\n"
    "\n"
    "\n"
    "def on_update(ctx):\n"
    "    # 기본 조작(방향키 이동·조사)은 엔진이 하고, Shift를 누르는 동안만 빨라지게 덧붙인다\n"
    "    if ctx.player:\n"
    "        ctx.player.speed = DASH_SPEED if ctx.input.held(\"dash\") else WALK_SPEED\n"
    "\n"
    "\n"
    "def add_combo(ctx):\n"
    "    # 이벤트의 \"스크립트 호출: game.py · add_combo()\"로 부른다. 반환값 → 변수로 받으면 조건 분기에 쓸 수 있다\n"
    "    global combo\n"
    "    combo += 1\n"
    "    return combo\n";

std::string toUtf8(const fs::path& path) {
    auto u8 = path.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

fs::path fromUtf8(const std::string& text) {
    return fs::path(std::u8string(text.begin(), text.end()));
}

// JSON 필드 읽기. 형식이 틀리면 nlohmann 예외 대신 한국어 메시지의 runtime_error를 던진다.
// (어느 필드가 무엇이어야 하는지 사용자에게 바로 알려 주기 위함)
[[noreturn]] void fieldError(const char* key, const char* expected) {
    throw std::runtime_error(std::string("'") + key + "'은(는) " + expected + "이어야 합니다");
}

const json& requireObject(const json& j, const char* what) {
    if (!j.is_object()) {
        throw std::runtime_error(std::string(what) + "은(는) 객체({ })여야 합니다");
    }
    return j;
}

std::string getString(const json& j, const char* key, const std::string& fallback) {
    auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    if (!it->is_string()) {
        fieldError(key, "문자열");
    }
    return it->get<std::string>();
}

std::string requireString(const json& j, const char* key) {
    if (!j.contains(key)) {
        throw std::runtime_error(std::string("'") + key + "' 항목이 없습니다");
    }
    return getString(j, key, std::string());
}

double getNumber(const json& j, const char* key, double fallback) {
    auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    if (!it->is_number()) {
        fieldError(key, "숫자");
    }
    return it->get<double>();
}

int getInt(const json& j, const char* key, int fallback) {
    auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    if (!it->is_number_integer()) {
        fieldError(key, "정수");
    }
    return it->get<int>();
}

bool getBool(const json& j, const char* key, bool fallback) {
    auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    if (!it->is_boolean()) {
        fieldError(key, "true/false");
    }
    return it->get<bool>();
}

const json& getArray(const json& j, const char* key) {
    static const json empty = json::array();
    auto it = j.find(key);
    if (it == j.end()) {
        return empty;
    }
    if (!it->is_array()) {
        fieldError(key, "배열");
    }
    return *it;
}

// 장면 ID는 파일 이름으로 쓰이므로 경로 문자를 허용하지 않는다.
bool isSafeId(const std::string& id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (char c : id) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool readJson(const fs::path& file, json& out, std::string& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        error = "파일을 열 수 없습니다";
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    out = json::parse(text, nullptr, false);
    if (out.is_discarded()) {
        error = "JSON 형식이 올바르지 않습니다";
        return false;
    }
    if (!out.is_object()) {
        error = "최상위 값이 객체가 아닙니다";
        return false;
    }
    return true;
}

// 임시 파일에 쓴 뒤 교체해서, 저장 도중 실패해도 기존 파일이 깨지지 않게 한다.
bool writeJson(const fs::path& file, const json& value, std::string& error) {
    fs::path temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "파일을 쓸 수 없습니다";
            return false;
        }
        out << value.dump(2) << '\n';
        if (!out) {
            error = "파일 쓰기 실패";
            return false;
        }
    }
    std::error_code ec;
    fs::rename(temp, file, ec);
    if (ec) {
        fs::remove(temp, ec);
        error = "파일 교체 실패";
        return false;
    }
    return true;
}

// 악의적이거나 손상된 파일이 끝없이 중첩되어 스택을 넘치지 않게 한다.
constexpr int kMaxEventDepth = 32;

bool facingKeyValid(const std::string& key) {
    return key == "up" || key == "down" || key == "left" || key == "right";
}

json valueToJson(const Value& v) {
    struct Visitor {
        json operator()(std::monostate) const { return nullptr; }
        json operator()(bool b) const { return b; }
        json operator()(std::int64_t i) const { return i; }
        json operator()(double d) const { return d; }
        json operator()(const std::string& s) const { return s; }
    };
    return std::visit(Visitor{}, v);
}

Value valueFromJson(const json& j) {
    if (j.is_null()) {
        return std::monostate{};
    }
    if (j.is_boolean()) {
        return j.get<bool>();
    }
    if (j.is_number_integer()) {
        return j.get<std::int64_t>();
    }
    if (j.is_number_float()) {
        return j.get<double>();
    }
    if (j.is_string()) {
        return j.get<std::string>();
    }
    throw std::runtime_error("값은 null·참거짓·숫자·문자열만 쓸 수 있습니다");
}

json commandsToJson(const std::vector<EventCommand>& commands);

json commandToJson(const EventCommand& c) {
    json j = {{"type", toKey(c.type)}};
    switch (c.type) {
    case EventCommandType::ShowChoices: {
        j["speaker"] = c.speaker;
        j["text"] = c.text;
        j["defaultChoice"] = c.defaultChoice;
        j["cancelChoice"] = c.cancelChoice;
        j["choices"] = json::array();
        for (const auto& choice : c.choices) {
            j["choices"].push_back({{"text", choice.text}, {"commands", commandsToJson(choice.commands)}});
        }
        break;
    }
    case EventCommandType::Say:
        j["speaker"] = c.speaker;
        j["text"] = c.text;
        break;
    case EventCommandType::Wait:
        j["seconds"] = c.seconds;
        break;
    case EventCommandType::SetVariable:
        j["variable"] = c.variable;
        j["op"] = toKey(c.op);
        j["value"] = valueToJson(c.value);
        break;
    case EventCommandType::IfVariable:
        j["variable"] = c.variable;
        j["compare"] = toKey(c.compare);
        j["value"] = valueToJson(c.value);
        j["then"] = commandsToJson(c.thenCommands);
        j["else"] = commandsToJson(c.elseCommands);
        break;
    case EventCommandType::Log:
    case EventCommandType::Comment:
        j["text"] = c.text;
        break;
    case EventCommandType::CallScript:
        j["script"] = c.script;
        j["function"] = c.function;
        if (!c.variable.empty()) {
            j["variable"] = c.variable; // 반환값을 넣을 변수
        }
        break;
    case EventCommandType::GetData:
        j["dataType"] = c.dataType;
        j["record"] = c.record;
        j["field"] = c.field;
        j["variable"] = c.variable;
        break;
    case EventCommandType::MoveObject:
        j["target"] = c.target;
        j["direction"] = c.direction;
        j["distance"] = c.distance;
        j["speed"] = c.speed;
        j["wait"] = c.waitForEnd;
        break;
    case EventCommandType::SetVisible:
        j["target"] = c.target;
        j["visible"] = c.show;
        break;
    case EventCommandType::ChangeScene:
        j["scene"] = c.scene;
        j["direction"] = c.direction;
        j["setPosition"] = c.setPosition;
        j["x"] = c.x;
        j["y"] = c.y;
        break;
    }
    return j;
}

json commandsToJson(const std::vector<EventCommand>& commands) {
    json list = json::array();
    for (const auto& c : commands) {
        list.push_back(commandToJson(c));
    }
    return list;
}

std::vector<EventCommand> commandsFromJson(const json& list, int depth);

EventCommand commandFromJson(const json& j, int depth) {
    requireObject(j, "이벤트 명령");
    EventCommand c;
    std::string typeKey = requireString(j, "type");
    auto type = commandTypeFromKey(typeKey);
    if (!type) {
        throw std::runtime_error("알 수 없는 이벤트 명령: " + typeKey);
    }
    c.type = *type;
    c.speaker = getString(j, "speaker", std::string());
    c.text = getString(j, "text", std::string());
    c.seconds = getNumber(j, "seconds", 0.0);
    c.variable = getString(j, "variable", std::string());
    if (j.contains("op")) {
        std::string key = getString(j, "op", std::string());
        auto op = variableOpFromKey(key);
        if (!op) {
            throw std::runtime_error("알 수 없는 변수 연산: " + key);
        }
        c.op = *op;
    }
    if (j.contains("compare")) {
        std::string key = getString(j, "compare", std::string());
        auto compare = compareOpFromKey(key);
        if (!compare) {
            throw std::runtime_error("알 수 없는 비교: " + key);
        }
        c.compare = *compare;
    }
    if (auto it = j.find("value"); it != j.end()) {
        c.value = valueFromJson(*it);
    }
    c.script = getString(j, "script", std::string());
    c.function = getString(j, "function", std::string("run"));
    c.dataType = getString(j, "dataType", std::string());
    c.record = getString(j, "record", std::string());
    c.field = getString(j, "field", std::string());
    c.target = getString(j, "target", c.target);
    c.direction = getString(j, "direction", c.direction);
    if (c.type == EventCommandType::ChangeScene) {
        c.direction = getString(j, "direction", std::string());
        if (!c.direction.empty() && !facingKeyValid(c.direction)) {
            throw std::runtime_error("알 수 없는 방향: " + c.direction);
        }
        c.scene = getString(j, "scene", std::string());
        c.setPosition = getBool(j, "setPosition", true);
        c.x = getNumber(j, "x", 0.0);
        c.y = getNumber(j, "y", 0.0);
    } else if (c.type == EventCommandType::MoveObject && !facingKeyValid(c.direction)) {
        throw std::runtime_error("알 수 없는 이동 방향: " + c.direction);
    }
    c.distance = getNumber(j, "distance", c.distance);
    c.speed = getNumber(j, "speed", c.speed);
    c.waitForEnd = getBool(j, "wait", c.waitForEnd);
    c.show = getBool(j, "visible", c.show);
    if (c.type == EventCommandType::IfVariable) {
        c.thenCommands = commandsFromJson(getArray(j, "then"), depth + 1);
        c.elseCommands = commandsFromJson(getArray(j, "else"), depth + 1);
    }
    if (c.type == EventCommandType::ShowChoices) {
        const auto& choices = getArray(j, "choices");
        if (choices.empty() || choices.size() > kMaxChoices) {
            throw std::runtime_error("선택지는 1~6개여야 합니다");
        }
        for (const auto& choice : choices) {
            requireObject(choice, "선택지");
            auto text = requireString(choice, "text");
            if (text.find_first_not_of(" \t\r\n") == std::string::npos || text.find_first_of("\r\n") != std::string::npos) {
                throw std::runtime_error("선택지 글자는 비어 있지 않은 한 줄이어야 합니다");
            }
            c.choices.push_back({text, commandsFromJson(getArray(choice, "commands"), depth + 1)});
        }
        c.defaultChoice = getInt(j, "defaultChoice", 0);
        c.cancelChoice = getInt(j, "cancelChoice", -1);
        if (c.defaultChoice < 0 || c.defaultChoice >= static_cast<int>(choices.size()) ||
            c.cancelChoice < -1 || c.cancelChoice >= static_cast<int>(choices.size())) {
            throw std::runtime_error("처음 선택·취소 선택지 번호가 범위를 벗어났습니다");
        }
    }
    return c;
}

std::vector<EventCommand> commandsFromJson(const json& list, int depth) {
    if (depth > kMaxEventDepth) {
        throw std::runtime_error("조건 분기·선택지 중첩이 너무 깊습니다");
    }
    if (!list.is_array()) {
        throw std::runtime_error("이벤트 명령 목록은 배열이어야 합니다");
    }
    std::vector<EventCommand> result;
    for (const auto& j : list) {
        result.push_back(commandFromJson(j, depth));
    }
    return result;
}

json objectToJson(const SceneObject& o) {
    json j = {{"id", o.id},     {"name", o.name},     {"x", o.x},         {"y", o.y},
              {"width", o.width}, {"height", o.height}, {"color", o.color}};
    if (o.passable) {
        j["passable"] = true;
    }
    if (!o.visible) {
        j["visible"] = false;
    }
    if (!(o.event == ObjectEvent{})) {
        json ej = {{"trigger", toKey(o.event.trigger)}};
        if (!o.event.conditions.empty()) {
            json conditions = json::array();
            for (const EventCondition& c : o.event.conditions) {
                conditions.push_back({{"variable", c.variable}, {"compare", toKey(c.compare)}, {"value", valueToJson(c.value)}});
            }
            ej["conditions"] = conditions;
        }
        if (o.event.repeat) {
            ej["repeat"] = true;
        }
        ej["commands"] = commandsToJson(o.event.commands);
        j["event"] = ej;
    }
    return j;
}

json sceneToJson(const Scene& s) {
    json objects = json::array();
    for (const auto& o : s.objects) {
        objects.push_back(objectToJson(o));
    }
    json j = {{"formatVersion", ProjectIO::kFormatVersion},
              {"id", s.id},
              {"name", s.name},
              {"width", s.width},
              {"height", s.height},
              {"background", s.background},
              {"objects", objects}};
    if (!s.playerObjectId.empty()) {
        j["player"] = s.playerObjectId;
    }
    return j;
}

// 형식 오류는 runtime_error로 올라오며 호출한 쪽에서 파일 이름과 함께 기록한다.
Scene sceneFromJson(const json& j) {
    Scene s;
    s.id = requireString(j, "id");
    s.name = getString(j, "name", s.id);
    s.width = getInt(j, "width", s.width);
    s.height = getInt(j, "height", s.height);
    s.background = getString(j, "background", s.background);
    for (const auto& oj : getArray(j, "objects")) {
        requireObject(oj, "장면 객체");
        SceneObject o;
        o.id = requireString(oj, "id");
        o.name = getString(oj, "name", o.id);
        o.x = getNumber(oj, "x", o.x);
        o.y = getNumber(oj, "y", o.y);
        o.width = getNumber(oj, "width", o.width);
        o.height = getNumber(oj, "height", o.height);
        o.color = getString(oj, "color", o.color);
        o.passable = getBool(oj, "passable", false);
        o.visible = getBool(oj, "visible", true);
        if (auto it = oj.find("event"); it != oj.end()) {
            const json& ej = requireObject(*it, "event");
            std::string triggerKey = getString(ej, "trigger", std::string("action"));
            auto trigger = triggerFromKey(triggerKey);
            if (!trigger) {
                throw std::runtime_error("알 수 없는 이벤트 실행 조건: " + triggerKey);
            }
            o.event.trigger = *trigger;
            for (const auto& cj : getArray(ej, "conditions")) {
                requireObject(cj, "이벤트 실행 조건");
                EventCondition c;
                c.variable = requireString(cj, "variable");
                if (c.variable.empty()) {
                    throw std::runtime_error("이벤트 실행 조건의 변수 이름이 비어 있습니다");
                }
                std::string compareKey = getString(cj, "compare", std::string("=="));
                auto compare = compareOpFromKey(compareKey);
                if (!compare) {
                    throw std::runtime_error("이벤트 실행 조건의 알 수 없는 비교: " + compareKey);
                }
                c.compare = *compare;
                auto value = cj.find("value");
                if (value == cj.end()) {
                    throw std::runtime_error("이벤트 실행 조건에 비교할 값이 없습니다: " + c.variable);
                }
                c.value = valueFromJson(*value);
                if (std::holds_alternative<std::monostate>(c.value)) {
                    throw std::runtime_error("이벤트 실행 조건의 값이 비어 있습니다: " + c.variable);
                }
                o.event.conditions.push_back(std::move(c));
            }
            o.event.repeat = getBool(ej, "repeat", false);
            o.event.commands = commandsFromJson(getArray(ej, "commands"), 0);
        }
        s.objects.push_back(std::move(o));
    }
    s.playerObjectId = getString(j, "player", std::string());
    if (!s.playerObjectId.empty() && !s.findObject(s.playerObjectId)) {
        throw std::runtime_error("플레이어로 지정한 객체가 장면에 없습니다: " + s.playerObjectId);
    }
    return s;
}

// ---------------------------------------------------------------- 데이터베이스

json fieldToJson(const FieldDef& f) {
    json j = {{"key", f.key}, {"name", f.name}, {"type", toKey(f.type)}, {"default", valueToJson(f.defaultValue)}};
    if (f.min) {
        j["min"] = *f.min;
    }
    if (f.max) {
        j["max"] = *f.max;
    }
    if (f.type == FieldType::Choice) {
        j["choices"] = f.choices;
    }
    if (f.type == FieldType::Reference) {
        j["refType"] = f.refType;
    }
    return j;
}

json dataTypeToJson(const DataType& t) {
    json fields = json::array();
    for (const auto& f : t.fields) {
        fields.push_back(fieldToJson(f));
    }
    json records = json::array();
    for (const auto& r : t.records) {
        json values = json::object();
        for (const auto& [key, value] : r.values) {
            values[key] = valueToJson(value);
        }
        records.push_back({{"id", r.id}, {"name", r.name}, {"values", values}});
    }
    return {{"formatVersion", ProjectIO::kFormatVersion},
            {"key", t.key},
            {"name", t.name},
            {"fields", fields},
            {"records", records}};
}

// 구조 오류(형식이 틀린 필드, 중복 키, 정의되지 않은 속성 값)는 예외로 거부한다.
// 값이 범위를 벗어나거나 참조가 끊긴 것은 Database::validate()가 경고로 보여 준다.
DataType dataTypeFromJson(const json& j) {
    DataType t;
    t.key = requireString(j, "key");
    t.name = getString(j, "name", t.key);
    for (const auto& fj : getArray(j, "fields")) {
        requireObject(fj, "속성 정의");
        FieldDef f;
        f.key = requireString(fj, "key");
        if (!isValidFieldKey(f.key)) {
            throw std::runtime_error("속성 키는 영문·숫자·_ 로 쓰고 id/name은 쓸 수 없습니다: " + f.key);
        }
        if (t.findField(f.key)) {
            throw std::runtime_error("속성 키가 중복됩니다: " + f.key);
        }
        f.name = getString(fj, "name", f.key);
        std::string typeKey = requireString(fj, "type");
        auto type = fieldTypeFromKey(typeKey);
        if (!type) {
            throw std::runtime_error("알 수 없는 속성 형식: " + typeKey);
        }
        f.type = *type;
        if (fj.contains("min")) {
            f.min = getNumber(fj, "min", 0.0);
        }
        if (fj.contains("max")) {
            f.max = getNumber(fj, "max", 0.0);
        }
        for (const auto& cj : getArray(fj, "choices")) {
            if (!cj.is_string()) {
                throw std::runtime_error("'choices'에는 문자열만 넣을 수 있습니다");
            }
            f.choices.push_back(cj.get<std::string>());
        }
        f.refType = getString(fj, "refType", std::string());
        f.defaultValue = fj.contains("default") ? valueFromJson(fj.at("default")) : emptyValueFor(f);
        t.fields.push_back(std::move(f));
    }
    for (const auto& rj : getArray(j, "records")) {
        requireObject(rj, "데이터 항목");
        DataRecord r;
        r.id = requireString(rj, "id");
        if (!isValidDataId(r.id)) {
            throw std::runtime_error("항목 ID에 쓸 수 없는 문자가 있습니다: " + r.id);
        }
        if (t.findRecord(r.id)) {
            throw std::runtime_error("항목 ID가 중복됩니다: " + r.id);
        }
        r.name = getString(rj, "name", r.id);
        if (auto it = rj.find("values"); it != rj.end()) {
            requireObject(*it, "'values'");
            for (const auto& [key, value] : it->items()) {
                if (!t.findField(key)) {
                    throw std::runtime_error("항목 " + r.id + "에 정의되지 않은 속성 값이 있습니다: " + key);
                }
                r.values[key] = valueFromJson(value);
            }
        }
        t.records.push_back(std::move(r));
    }
    return t;
}

bool checkVersion(const json& j, const std::string& file, std::vector<ProjectError>& errors) {
    auto it = j.find("formatVersion");
    int version = it != j.end() && it->is_number_integer() ? it->get<int>() : 0;
    if (version < 1) {
        errors.push_back({file, "formatVersion이 없습니다"});
        return false;
    }
    if (version > ProjectIO::kFormatVersion) {
        errors.push_back({file, "이 Naru Studio보다 새로운 형식(" + std::to_string(version) +
                                    ")입니다. 프로그램을 업데이트하세요"});
        return false;
    }
    return true;
}

} // namespace

std::string ProjectLoadResult::errorText() const {
    std::string text;
    for (const auto& e : errors) {
        if (!text.empty()) {
            text += "\n";
        }
        text += (e.file.empty() ? std::string() : e.file + ": ") + e.message;
    }
    return text;
}

bool ProjectIO::isProjectDirectory(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / kProjectFile, ec);
}

ProjectLoadResult ProjectIO::create(const fs::path& dir, const std::string& name) {
    ProjectLoadResult result;
    std::error_code ec;
    if (fs::exists(dir, ec) && !fs::is_empty(dir, ec)) {
        result.errors.push_back({"", "폴더가 비어 있지 않습니다: " + toUtf8(dir)});
        return result;
    }
    for (const auto& sub : {dir, scenesDirectory(dir), scriptsDirectory(dir), assetsDirectory(dir)}) {
        fs::create_directories(sub, ec);
        if (ec) {
            result.errors.push_back({"", "폴더를 만들 수 없습니다: " + toUtf8(sub)});
            return result;
        }
    }

    Project project;
    project.name = name;
    Scene scene;
    scene.id = project.newId("scene");
    scene.name = "시작 장면";
    project.startSceneId = scene.id;
    project.scenes.push_back(scene);

    auto addObject = [&project](const char* objectName, double x, double y, const char* color) {
        SceneObject o;
        o.id = project.newId("obj");
        o.name = objectName;
        o.x = x;
        o.y = y;
        o.color = color;
        project.scenes.front().objects.push_back(o);
    };
    addObject("주인공", 320, 400, "#e0a040");
    project.scenes.front().playerObjectId = project.scenes.front().objects.back().id;
    project.globalScripts = {"game.py"};
    project.variables.push_back({"box_opened", std::int64_t{0}, "상자를 연 횟수 (상자 이벤트에서 1씩 더함)"});
    addObject("상자", 640, 400, "#8a6a40");

    // 이벤트 편집기 사용 예: 대사 → 변수 더하기 → 조건 분기
    EventCommand say;
    say.type = EventCommandType::Say;
    say.speaker = "상자";
    say.text = "낡은 나무 상자다.";
    EventCommand count;
    count.type = EventCommandType::SetVariable;
    count.variable = "box_opened";
    count.op = VariableOp::Add;
    count.value = std::int64_t{1};
    EventCommand again;
    again.type = EventCommandType::Say;
    again.text = "이미 열어 본 상자다.";
    EventCommand first;
    first.type = EventCommandType::Log;
    first.text = "상자를 처음 열었다";
    EventCommand branch;
    branch.type = EventCommandType::IfVariable;
    branch.variable = "box_opened";
    branch.compare = CompareOp::GreaterEqual;
    branch.value = std::int64_t{2};
    branch.thenCommands = {again};
    branch.elseCommands = {first};
    project.scenes.front().objects.back().event.commands = {say, count, branch};

    // 데이터베이스 예제: 템플릿에서 만든 아이템·몬스터 (속성은 자유롭게 바꿀 수 있다)
    DataType items = makeTemplate(DataTemplate::Item, project.database);
    items.records = {
        {"potion", "포션", {{"category", std::string("소모품")}, {"price", std::int64_t{50}}, {"heal", std::int64_t{50}}}},
        {"ether", "에테르", {{"category", std::string("소모품")}, {"price", std::int64_t{120}}, {"heal", std::int64_t{0}}}},
    };
    project.database.types.push_back(std::move(items));
    DataType monsters = makeTemplate(DataTemplate::Monster, project.database);
    monsters.records = {
        {"slime", "슬라임", {{"hp", std::int64_t{30}}, {"element", std::string("물")}, {"drop", std::string("potion")}}},
    };
    project.database.types.push_back(std::move(monsters));

    result.errors = save(project, dir);
    if (!result.errors.empty()) {
        return result;
    }
    std::ofstream script(scriptsDirectory(dir) / "main.py", std::ios::binary);
    script << kSampleScript;
    std::ofstream globalScript(scriptsDirectory(dir) / "game.py", std::ios::binary);
    globalScript << kSampleGlobalScript;
    result.project = std::move(project);
    return result;
}

bool ProjectIO::isValidScriptName(const std::string& name) {
    return name.size() > 3 && name.size() <= 128 && name.ends_with(".py") && name.find_first_of("/\\:") == std::string::npos &&
           name.find("..") == std::string::npos;
}

ProjectLoadResult ProjectIO::load(const fs::path& dir) {
    ProjectLoadResult result;
    json root;
    std::string error;
    if (!readJson(dir / kProjectFile, root, error)) {
        result.errors.push_back({kProjectFile, error});
        return result;
    }
    if (!checkVersion(root, kProjectFile, result.errors)) {
        return result;
    }

    Project project;
    try {
        project.name = getString(root, "name", "이름 없는 프로젝트");
        project.startSceneId = getString(root, "startScene", std::string());
        for (const auto& scriptJson : getArray(root, "globalScripts")) {
            if (!scriptJson.is_string()) {
                throw std::runtime_error("'globalScripts'에는 스크립트 파일 이름 문자열만 넣을 수 있습니다");
            }
            std::string script = scriptJson.get<std::string>();
            if (!ProjectIO::isValidScriptName(script)) {
                result.errors.push_back({kProjectFile, "전역 스크립트 이름이 올바르지 않습니다 (scripts/ 안의 .py 파일 이름): " + script});
            } else if (std::find(project.globalScripts.begin(), project.globalScripts.end(), script) !=
                       project.globalScripts.end()) {
                result.errors.push_back({kProjectFile, "전역 스크립트가 중복 등록되었습니다: " + script});
            } else {
                project.globalScripts.push_back(script);
            }
        }
        for (const auto& vj : getArray(root, "variables")) {
            if (!vj.is_object()) {
                throw std::runtime_error("'variables'에는 변수 정의 객체만 넣을 수 있습니다");
            }
            VariableDef def;
            def.name = getString(vj, "name", std::string());
            def.description = getString(vj, "description", std::string());
            auto value = vj.find("value");
            if (value != vj.end()) {
                def.initial = valueFromJson(*value);
            }
            if (def.name.empty() || def.name.size() > 64) {
                result.errors.push_back({kProjectFile, "변수 이름이 비어 있거나 너무 깁니다 (64자까지)"});
            } else if (project.findVariable(def.name)) {
                result.errors.push_back({kProjectFile, "변수 이름이 중복됩니다: " + def.name});
            } else if (std::holds_alternative<std::monostate>(def.initial)) {
                result.errors.push_back({kProjectFile, "변수 '" + def.name + "'의 처음 값이 없습니다 (정수·소수·글자·참거짓)"});
            } else {
                project.variables.push_back(std::move(def));
            }
        }
        std::set<std::string> objectIds;
        for (const auto& idJson : getArray(root, "scenes")) {
            if (!idJson.is_string()) {
                throw std::runtime_error("'scenes'에는 장면 ID 문자열만 넣을 수 있습니다");
            }
            std::string sceneId = idJson.get<std::string>();
            std::string file = "scenes/" + sceneId + ".json";
            if (!isSafeId(sceneId)) {
                result.errors.push_back({kProjectFile, "장면 ID에 쓸 수 없는 문자가 있습니다: " + sceneId});
                continue;
            }
            if (project.findScene(sceneId)) {
                result.errors.push_back({kProjectFile, "장면 ID가 중복됩니다: " + sceneId});
                continue;
            }
            json sj;
            if (!readJson(dir / fromUtf8(file), sj, error)) {
                result.errors.push_back({file, error});
                continue;
            }
            if (!checkVersion(sj, file, result.errors)) {
                continue;
            }
            try {
                Scene scene = sceneFromJson(sj);
                if (scene.id != sceneId) {
                    result.errors.push_back({file, "파일 안의 id(" + scene.id + ")가 project.json과 다릅니다"});
                    continue;
                }
                for (const auto& o : scene.objects) {
                    if (!objectIds.insert(o.id).second) {
                        result.errors.push_back({file, "객체 ID가 중복됩니다: " + o.id});
                    }
                }
                project.scenes.push_back(std::move(scene));
            } catch (const std::exception& e) {
                result.errors.push_back({file, std::string("형식 오류: ") + e.what()});
            }
        }

        for (const auto& keyJson : getArray(root, "dataTypes")) {
            if (!keyJson.is_string()) {
                throw std::runtime_error("'dataTypes'에는 데이터 종류 키 문자열만 넣을 수 있습니다");
            }
            std::string key = keyJson.get<std::string>();
            std::string file = "data/" + key + ".json";
            if (!isValidDataId(key)) {
                result.errors.push_back({kProjectFile, "데이터 종류 키에 쓸 수 없는 문자가 있습니다: " + key});
                continue;
            }
            if (project.database.findType(key)) {
                result.errors.push_back({kProjectFile, "데이터 종류 키가 중복됩니다: " + key});
                continue;
            }
            json dj;
            if (!readJson(dir / fromUtf8(file), dj, error)) {
                result.errors.push_back({file, error});
                continue;
            }
            if (!checkVersion(dj, file, result.errors)) {
                continue;
            }
            try {
                DataType type = dataTypeFromJson(dj);
                if (type.key != key) {
                    result.errors.push_back({file, "파일 안의 key(" + type.key + ")가 project.json과 다릅니다"});
                    continue;
                }
                project.database.types.push_back(std::move(type));
            } catch (const std::exception& e) {
                result.errors.push_back({file, std::string("형식 오류: ") + e.what()});
            }
        }
    } catch (const std::exception& e) {
        result.errors.push_back({kProjectFile, std::string("형식 오류: ") + e.what()});
    }

    if (!project.startSceneId.empty() && !project.findScene(project.startSceneId)) {
        result.errors.push_back({kProjectFile, "시작 장면이 없습니다: " + project.startSceneId});
    }
    if (result.errors.empty()) {
        result.project = std::move(project);
    }
    return result;
}

std::vector<ProjectError> ProjectIO::save(const Project& project, const fs::path& dir) {
    std::vector<ProjectError> errors;
    std::error_code ec;
    fs::create_directories(scenesDirectory(dir), ec);

    json sceneIds = json::array();
    std::set<fs::path> written;
    for (const auto& scene : project.scenes) {
        std::string file = "scenes/" + scene.id + ".json";
        if (!isSafeId(scene.id)) {
            errors.push_back({file, "장면 ID에 쓸 수 없는 문자가 있습니다"});
            continue;
        }
        std::string error;
        fs::path path = dir / fromUtf8(file);
        if (!writeJson(path, sceneToJson(scene), error)) {
            errors.push_back({file, error});
        }
        written.insert(path.filename());
        sceneIds.push_back(scene.id);
    }

    fs::create_directories(dataDirectory(dir), ec);
    json dataKeys = json::array();
    std::set<fs::path> writtenData;
    for (const auto& type : project.database.types) {
        std::string file = "data/" + type.key + ".json";
        if (!isValidDataId(type.key)) {
            errors.push_back({file, "데이터 종류 키에 쓸 수 없는 문자가 있습니다"});
            continue;
        }
        std::string error;
        fs::path path = dir / fromUtf8(file);
        if (!writeJson(path, dataTypeToJson(type), error)) {
            errors.push_back({file, error});
        }
        writtenData.insert(path.filename());
        dataKeys.push_back(type.key);
    }

    json root = {{"formatVersion", kFormatVersion},
                 {"name", project.name},
                 {"startScene", project.startSceneId},
                 {"scenes", sceneIds},
                 {"dataTypes", dataKeys},
                 {"globalScripts", project.globalScripts}};
    json variables = json::array();
    for (const auto& v : project.variables) {
        json vj = {{"name", v.name}, {"value", valueToJson(v.initial)}};
        if (!v.description.empty()) {
            vj["description"] = v.description;
        }
        variables.push_back(vj);
    }
    root["variables"] = variables;
    std::string error;
    if (!writeJson(dir / kProjectFile, root, error)) {
        errors.push_back({kProjectFile, error});
    }

    // 모든 저장이 성공했을 때만, 프로젝트에서 삭제한 장면·데이터 종류의 파일을 정리한다.
    if (errors.empty()) {
        auto cleanup = [&ec](const fs::path& folder, const std::set<fs::path>& keep) {
            for (const auto& entry : fs::directory_iterator(folder, ec)) {
                if (entry.path().extension() == ".json" && !keep.contains(entry.path().filename())) {
                    fs::remove(entry.path(), ec);
                }
            }
        };
        cleanup(scenesDirectory(dir), written);
        cleanup(dataDirectory(dir), writtenData);
    }
    return errors;
}

} // namespace naru
