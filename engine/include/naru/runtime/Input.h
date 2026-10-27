#pragma once

#include <map>
#include <string>
#include <vector>

namespace naru {

// 엔진이 기본 조작에 쓰는 입력 동작. 스크립트는 이 밖에도 이름으로 새 동작("dash" 등)을 만들 수 있다.
enum class InputAction { Up, Down, Left, Right, Confirm, Cancel };

// 동작 이름 ("up", "down", "left", "right", "confirm", "cancel").
const char* toKey(InputAction action);

// 한 프레임의 입력 상태. 동작 이름별로 held(누르고 있는 중)와 pressed(이번 프레임에 새로 누름)를 가진다.
class InputState {
public:
    void press(const std::string& action) { set(action, true); }
    void release(const std::string& action) { set(action, false); }
    // 누름 상태를 정한다. 떼어 있다가 눌리면 pressed가 된다.
    void set(const std::string& action, bool held);
    // 프레임을 처리한 뒤 호출한다. "새로 누름"만 지운다.
    void endFrame();
    // 이 동작의 "새로 누름"을 이미 쓴 것으로 지운다 (예: 대사를 넘긴 결정 키).
    void consume(const std::string& action);

    bool isHeld(const std::string& action) const;
    bool wasPressed(const std::string& action) const;

    void press(InputAction action) { press(std::string(toKey(action))); }
    void release(InputAction action) { release(std::string(toKey(action))); }
    bool isHeld(InputAction action) const { return isHeld(std::string(toKey(action))); }
    bool wasPressed(InputAction action) const { return wasPressed(std::string(toKey(action))); }

private:
    struct Entry {
        bool held = false;
        bool pressed = false;
    };
    std::map<std::string, Entry> entries_;
};

// 키 배치: 동작 이름 → 키 이름 목록. 키 이름은 player가 쓰는 SDL 키 이름이다
// ("Z", "Return", "Space", "Up", "Left Shift" ...). 대소문자를 가리지 않으며
// "Enter"(=Return), "Esc"(=Escape), "Shift"/"Ctrl"/"Alt"(왼쪽·오른쪽 모두) 같은 별칭을 받는다.
class InputMap {
public:
    // 엔진 기본 배치: 방향키·WASD 이동, Z/Enter/Space 결정, X/Esc 취소.
    static InputMap defaults();

    // 동작의 키 목록을 바꾼다 (빈 목록이면 그 동작을 끈다). 새 동작 이름이면 추가된다.
    void bind(const std::string& action, std::vector<std::string> keys);
    const std::vector<std::string>& keys(const std::string& action) const;
    std::vector<std::string> actions() const;
    // 이 키(실제 눌린 키 이름)를 쓰는 동작들.
    std::vector<std::string> actionsForKey(const std::string& keyName) const;

    // 배치된 키 이름이 실제 키 이름과 맞는가 (별칭 포함).
    static bool keyMatches(const std::string& bound, const std::string& keyName);
    // 동작 이름 규칙: 영문 소문자로 시작, 영문 소문자·숫자·_ (최대 32자).
    static bool isValidActionName(const std::string& action);

private:
    std::map<std::string, std::vector<std::string>> bindings_;
};

} // namespace naru
