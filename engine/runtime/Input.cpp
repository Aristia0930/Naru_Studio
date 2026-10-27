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
    for (const auto& [action, keys] : bindings_) {
        for (const std::string& bound : keys) {
            if (keyMatches(bound, keyName)) {
                result.push_back(action);
                break;
            }
        }
    }
    return result;
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

} // namespace naru
