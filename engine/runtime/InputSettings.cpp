#include "naru/runtime/Input.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace naru {
namespace {
using nlohmann::json;
struct Range { std::size_t begin, end; };

// 설정 파일은 세 변수의 Python 리터럴 대입이다. import/함수/식은 실행하거나 평가하지 않는다.
class Reader {
public:
    explicit Reader(const std::string& source) : source_(source) {}
    std::map<std::string, json> read() {
        if (source_.size() > 256 * 1024) fail("설정 파일이 너무 큽니다");
        if (source_.starts_with("\xef\xbb\xbf")) position_ = 3;
        std::map<std::string, json> result;
        skip();
        while (position_ < source_.size()) {
            auto begin = position_;
            while (position_ < source_.size() &&
                   (std::isalnum(static_cast<unsigned char>(source_[position_])) || source_[position_] == '_')) ++position_;
            auto name = source_.substr(begin, position_ - begin);
            if (name != "DEFAULT_CONTROL_ENABLED" && name != "PLAYER_SPEED" && name != "KEY_BINDINGS")
                fail("DEFAULT_CONTROL_ENABLED, PLAYER_SPEED, KEY_BINDINGS의 값만 설정하세요. 동작 코드는 game.py에 작성하세요");
            if (result.contains(name)) fail("설정 변수가 중복됩니다: " + name);
            expect('=');
            skip();
            begin = position_;
            result[name] = value(0);
            ranges[name] = {begin, position_};
            // 한 대입문은 줄 끝에서 끝난다. 다음 변수 앞의 공백/주석은 skip()이 처리한다.
            while (position_ < source_.size() && (source_[position_] == ' ' || source_[position_] == '\t' || source_[position_] == '\r')) ++position_;
            if (position_ < source_.size() && source_[position_] != '\n' && source_[position_] != '#')
                fail("설정값 뒤에는 주석이나 줄바꿈만 쓸 수 있습니다");
            skip();
        }
        for (const auto* name : {"DEFAULT_CONTROL_ENABLED", "PLAYER_SPEED", "KEY_BINDINGS"})
            if (!result.contains(name)) fail(std::string("설정 변수가 없습니다: ") + name);
        return result;
    }
    int line() const { return 1 + static_cast<int>(std::count(source_.begin(), source_.begin() + position_, '\n')); }
    int lineOf(const std::string& field) const {
        const auto found = ranges.find(field);
        return found == ranges.end() ? line() : 1 + static_cast<int>(std::count(source_.begin(), source_.begin() + found->second.begin, '\n'));
    }
    std::map<std::string, Range> ranges;
private:
    [[noreturn]] void fail(const std::string& message) const { throw std::runtime_error(message); }
    void skip() {
        while (position_ < source_.size()) {
            if (std::isspace(static_cast<unsigned char>(source_[position_]))) { ++position_; continue; }
            if (source_[position_] == '#') {
                while (position_ < source_.size() && source_[position_] != '\n') ++position_;
                continue;
            }
            break;
        }
    }
    void expect(char c) {
        skip();
        if (position_ >= source_.size() || source_[position_] != c) fail(std::string("필요한 기호: ") + c);
        ++position_;
    }
    bool take(char c) {
        skip();
        if (position_ < source_.size() && source_[position_] == c) { ++position_; return true; }
        return false;
    }
    std::string string() {
        skip();
        if (position_ >= source_.size() || (source_[position_] != '\'' && source_[position_] != '"')) fail("키와 동작 이름은 따옴표로 감싸세요");
        char quote = source_[position_++];
        std::string encoded = "\"";
        while (position_ < source_.size()) {
            char c = source_[position_++];
            if (c == quote) {
                encoded += '"';
                return json::parse(encoded).get<std::string>();
            }
            if (c == '\n' || c == '\r') fail("문자열은 한 줄로 작성하세요");
            if (c == '\\') {
                if (position_ >= source_.size()) fail("문자열의 끝이 올바르지 않습니다");
                char escaped = source_[position_++];
                if (escaped == '\'') encoded += '\'';
                else { encoded += '\\'; encoded += escaped; }
            } else if (c == '"') encoded += "\\\"";
            else encoded += c;
        }
        fail("문자열을 닫는 따옴표가 없습니다");
    }
    json value(int depth) {
        if (depth > 8) fail("설정값의 중첩이 너무 깊습니다");
        skip();
        if (position_ >= source_.size()) fail("설정값이 없습니다");
        char c = source_[position_];
        if (c == '\'' || c == '"') return string();
        if (take('{')) {
            json object = json::object();
            if (take('}')) return object;
            do {
                std::string key = string();
                if (object.contains(key)) fail("중복된 동작 이름: " + key);
                expect(':');
                object[key] = value(depth + 1);
                if (take('}')) return object;
                expect(',');
            } while (!take('}'));
            return object;
        }
        if (take('[')) {
            json list = json::array();
            if (take(']')) return list;
            do {
                list.push_back(value(depth + 1));
                if (take(']')) return list;
                expect(',');
            } while (!take(']'));
            return list;
        }
        if (source_.compare(position_, 4, "True") == 0) { position_ += 4; return true; }
        if (source_.compare(position_, 5, "False") == 0) { position_ += 5; return false; }
        auto begin = position_;
        while (position_ < source_.size() && std::string("0123456789+-.eE").find(source_[position_]) != std::string::npos) ++position_;
        if (begin == position_) fail("참/거짓(True/False), 숫자, 문자열, 목록, 사전만 쓸 수 있습니다");
        return json::parse(source_.substr(begin, position_ - begin));
    }
    const std::string& source_;
    std::size_t position_ = 0;
};

InputSettings checked(const std::map<std::string, json>& values, std::string* field = nullptr) {
    InputSettings result;
    if (field) *field = "DEFAULT_CONTROL_ENABLED";
    if (!values.at("DEFAULT_CONTROL_ENABLED").is_boolean()) throw std::runtime_error("DEFAULT_CONTROL_ENABLED는 True 또는 False여야 합니다");
    result.defaultControl = values.at("DEFAULT_CONTROL_ENABLED").get<bool>();
    if (field) *field = "PLAYER_SPEED";
    const auto& speed = values.at("PLAYER_SPEED");
    if (!speed.is_number()) throw std::runtime_error("PLAYER_SPEED에는 숫자를 쓰세요");
    result.playerSpeed = speed.get<double>();
    if (!std::isfinite(result.playerSpeed) || result.playerSpeed < 0 || result.playerSpeed > 100000)
        throw std::runtime_error("PLAYER_SPEED는 0~100000 픽셀/초여야 합니다");
    if (field) *field = "KEY_BINDINGS";
    const auto& bindings = values.at("KEY_BINDINGS");
    if (!bindings.is_object() || bindings.size() > 128) throw std::runtime_error("KEY_BINDINGS는 동작 이름과 키 목록의 사전이어야 합니다 (128개까지)");
    for (const auto& action : InputMap::defaults().actions())
        if (!bindings.contains(action)) throw std::runtime_error("기본 동작이 없습니다: " + action + " (끄려면 [] 사용)");
    result.bindings.clear();
    for (const auto& [action, keys] : bindings.items()) {
        if (!InputMap::isValidActionName(action)) throw std::runtime_error("잘못된 동작 이름: " + action);
        if (!keys.is_array() || keys.size() > 64) throw std::runtime_error(action + ": 키는 문자열 목록이어야 합니다 (64개까지)");
        std::set<std::string> used;
        auto& target = result.bindings[action];
        for (const auto& key : keys) {
            if (!key.is_string()) throw std::runtime_error(action + ": 키 이름은 문자열이어야 합니다");
            auto canonical = InputMap::canonicalBinding(key.get<std::string>());
            if (!canonical) throw std::runtime_error(action + ": 잘못된 키/조합 (서로 다른 키 1~4개): " + key.get<std::string>());
            if (!used.insert(*canonical).second) throw std::runtime_error(action + ": 중복된 키: " + *canonical);
            target.push_back(*canonical);
        }
    }
    return result;
}
}

InputSettingsResult parseInputSettings(const std::string& source) {
    Reader reader(source);
    std::string field;
    try { return {checked(reader.read(), &field), {}, 0}; }
    catch (const std::exception& error) { return {{}, error.what(), field.empty() ? reader.line() : reader.lineOf(field)}; }
}

std::string inputSettingsSource(const InputSettings& settings, const std::string& previous) {
    std::map<std::string, std::string> values = {
        {"DEFAULT_CONTROL_ENABLED", settings.defaultControl ? "True" : "False"},
        {"PLAYER_SPEED", json(settings.playerSpeed).dump()},
        {"KEY_BINDINGS", json(settings.bindings).dump(4, ' ', false, json::error_handler_t::replace)}};
    if (previous.empty()) {
        return "# 키 설정 화면과 이 파일은 같은 기본 설정입니다. 한쪽을 바꾸면 다른 쪽도 반영됩니다.\n"
               "# True = ON, False = OFF. 동작의 키 목록 [] = 그 동작 OFF.\n"
               "# 숫자·문자열·목록·사전만 적으세요. 함수나 실행 코드는 game.py에 작성합니다.\n"
               "# 게임 실행 중의 ctx.input.bind/ctx.player 변경은 실행 세션에만 적용됩니다.\n\n"
               "DEFAULT_CONTROL_ENABLED = " + values.at("DEFAULT_CONTROL_ENABLED") + "\n"
               "PLAYER_SPEED = " + values.at("PLAYER_SPEED") + "\n"
               "KEY_BINDINGS = " + values.at("KEY_BINDINGS") + "\n";
    }
    Reader reader(previous);
    const auto original = checked(reader.read());
    std::vector<std::pair<std::string, Range>> ranges(reader.ranges.begin(), reader.ranges.end());
    std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) { return a.second.begin > b.second.begin; });
    std::string result = previous;
    for (const auto& [name, range] : ranges) {
        const bool changed = name == "DEFAULT_CONTROL_ENABLED" ? original.defaultControl != settings.defaultControl
            : name == "PLAYER_SPEED" ? original.playerSpeed != settings.playerSpeed : original.bindings != settings.bindings;
        if (changed) result.replace(range.begin, range.end - range.begin, values.at(name));
    }
    return result;
}
} // namespace naru
