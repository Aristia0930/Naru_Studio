#include "naru/runtime/Input.h"

#include <algorithm>
#include <cctype>

namespace naru {

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

bool splitBinding(const std::string& text, std::vector<std::string>& result) {
    if (result.size() >= 4) return false;
    if (auto key = InputMap::canonicalKeyName(trim(text))) {
        result.push_back(*key);
        return true;
    }
    // 먼저 키 전체를 검사하므로 Keypad +를 조합 구분자로 잘못 나누지 않는다.
    for (auto at = text.find('+'); at != std::string::npos; at = text.find('+', at + 1)) {
        auto key = InputMap::canonicalKeyName(trim(text.substr(0, at)));
        if (!key) continue;
        result.push_back(*key);
        if (splitBinding(text.substr(at + 1), result)) return true;
        result.pop_back();
    }
    return false;
}

int modifierOrder(const std::string& key) {
    if (key == "Ctrl" || key == "Left Ctrl" || key == "Right Ctrl") return 0;
    if (key == "Shift" || key == "Left Shift" || key == "Right Shift") return 1;
    if (key == "Alt" || key == "Left Alt" || key == "Right Alt") return 2;
    return 3;
}

} // namespace

const char* toKey(InputAction action) {
    switch (action) {
    case InputAction::Up: return "up";
    case InputAction::Down: return "down";
    case InputAction::Left: return "left";
    case InputAction::Right: return "right";
    case InputAction::Confirm: return "confirm";
    case InputAction::Cancel: return "cancel";
    }
    return "";
}

void InputState::set(const std::string& action, bool held) {
    Entry& e = entries_[action];
    if (held && !e.held) {
        e.pressed = true;
    }
    e.held = held;
}

void InputState::endFrame() {
    for (auto& [name, e] : entries_) {
        e.pressed = false;
    }
}

void InputState::consume(const std::string& action) {
    if (auto it = entries_.find(action); it != entries_.end()) {
        it->second.pressed = false;
    }
}

bool InputState::isHeld(const std::string& action) const {
    auto it = entries_.find(action);
    return it != entries_.end() && it->second.held;
}

bool InputState::wasPressed(const std::string& action) const {
    auto it = entries_.find(action);
    return it != entries_.end() && it->second.pressed;
}

InputMap InputMap::defaults() {
    InputMap map;
    map.bind("up", {"Up", "W"});
    map.bind("down", {"Down", "S"});
    map.bind("left", {"Left", "A"});
    map.bind("right", {"Right", "D"});
    map.bind("confirm", {"Z", "Return", "Space", "Keypad Enter"});
    map.bind("cancel", {"X", "Escape"});
    return map;
}

void InputMap::bind(const std::string& action, std::vector<std::string> keys) {
    auto& compiled = compiledBindings_[action];
    compiled.clear();
    for (auto& key : keys) {
        auto parts = bindingKeys(key);
        if (parts.empty()) parts.push_back(key); // 기존 C++/스크립트의 확장 키 문자열도 보존
        else key = *canonicalBinding(key);
        compiled.push_back(std::move(parts));
    }
    bindings_[action] = std::move(keys);
}

const std::vector<std::string>& InputMap::keys(const std::string& action) const {
    static const std::vector<std::string> none;
    auto it = bindings_.find(action);
    return it == bindings_.end() ? none : it->second;
}

std::vector<std::string> InputMap::actions() const {
    std::vector<std::string> names;
    for (const auto& [name, keys] : bindings_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> InputMap::actionsForKey(const std::string& keyName) const {
    std::vector<std::string> result;
    for (const auto& [action, keys] : bindings_) if (actionHeld(action, {keyName})) result.push_back(action);
    return result;
}

bool InputMap::actionHeld(const std::string& action, const std::set<std::string>& physicalKeys) const {
    auto found = compiledBindings_.find(action);
    if (found == compiledBindings_.end()) return false;
    for (const auto& combination : found->second) {
        if (!combination.empty() && std::all_of(combination.begin(), combination.end(), [&](const auto& required) {
            return std::any_of(physicalKeys.begin(), physicalKeys.end(), [&](const auto& held) { return keyMatches(required, held); });
        })) return true;
    }
    return false;
}

bool InputMap::keyMatches(const std::string& bound, const std::string& keyName) {
    std::string b = lower(bound);
    std::string k = lower(keyName);
    if (b == k) {
        return true;
    }
    if (b == "enter") {
        return k == "return";
    }
    if (b == "esc") {
        return k == "escape";
    }
    // "shift"는 "left shift"·"right shift" 모두
    if (b == "shift" || b == "ctrl" || b == "alt") {
        return k == "left " + b || k == "right " + b;
    }
    return false;
}

bool InputMap::isValidActionName(const std::string& action) {
    if (action.empty() || action.size() > 32 || !(action[0] >= 'a' && action[0] <= 'z')) {
        return false;
    }
    return std::all_of(action.begin(), action.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

const std::vector<std::string>& InputMap::keyNames() {
    static const auto names = [] {
        std::vector<std::string> result = {"Up", "Down", "Left", "Right", "Return", "Escape", "Space", "Tab",
            "Backspace", "Insert", "Delete", "Home", "End", "PageUp", "PageDown", "Shift", "Ctrl", "Alt",
            "Left Shift", "Right Shift", "Left Ctrl", "Right Ctrl", "Left Alt", "Right Alt",
            "CapsLock", "Numlock", "ScrollLock", "PrintScreen", "Pause", "Keypad Enter",
            "Keypad +", "Keypad -", "Keypad *", "Keypad /", "Keypad .", "Keypad =",
            "'", ",", "-", ".", "/", ";", "=", "[", "\\", "]", "`"};
        for (char c = 'A'; c <= 'Z'; ++c) result.emplace_back(1, c);
        for (char c = '0'; c <= '9'; ++c) {
            result.emplace_back(1, c);
            result.push_back("Keypad " + std::string(1, c));
        }
        for (int n = 1; n <= 24; ++n) result.push_back("F" + std::to_string(n));
        return result;
    }();
    return names;
}

std::optional<std::string> InputMap::canonicalKeyName(const std::string& key) {
    const auto name = lower(key);
    if (name == "enter") return "Return";
    if (name == "esc") return "Escape";
    for (const auto& candidate : keyNames()) if (lower(candidate) == name) return candidate;
    return std::nullopt;
}

std::vector<std::string> InputMap::bindingKeys(const std::string& binding) {
    if (binding.size() > 256) return {};
    std::vector<std::string> result;
    if (!splitBinding(trim(binding), result)) return {};
    for (std::size_t i = 0; i < result.size(); ++i)
        for (std::size_t j = i + 1; j < result.size(); ++j)
            if (keyMatches(result[i], result[j]) || keyMatches(result[j], result[i])) return {};
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return modifierOrder(a) != modifierOrder(b) ? modifierOrder(a) < modifierOrder(b) : a < b;
    });
    return result;
}

std::optional<std::string> InputMap::canonicalBinding(const std::string& binding) {
    auto parts = bindingKeys(binding);
    if (parts.empty()) return std::nullopt;
    std::string result;
    for (const auto& key : parts) { if (!result.empty()) result += " + "; result += key; }
    return result;
}

InputSettings::InputSettings() {
    auto map = InputMap::defaults();
    for (const auto& action : map.actions()) bindings[action] = map.keys(action);
}

} // namespace naru
