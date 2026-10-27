#pragma once

#include <array>
#include <cstddef>

namespace naru {

// 게임이 다루는 입력 동작. 키보드·게임패드의 실제 키는 실행 화면(player)이 이 동작으로 바꿔 넘긴다.
// 키 배치를 프로젝트 설정·스크립트로 바꾸는 방식은 아직 정하지 않았다 (docs/ROADMAP.md 미결정 사항).
enum class InputAction { Up, Down, Left, Right, Confirm, Cancel, Count };

// 한 프레임의 입력 상태. held: 누르고 있는 중, pressed: 이번 프레임에 새로 누름.
class InputState {
public:
    void press(InputAction action) {
        if (!held_[index(action)]) {
            pressed_[index(action)] = true;
        }
        held_[index(action)] = true;
    }
    void release(InputAction action) { held_[index(action)] = false; }
    // 프레임을 처리한 뒤 호출한다. "새로 누름"만 지운다.
    void endFrame() { pressed_.fill(false); }

    bool isHeld(InputAction action) const { return held_[index(action)]; }
    bool wasPressed(InputAction action) const { return pressed_[index(action)]; }

private:
    static constexpr std::size_t kCount = static_cast<std::size_t>(InputAction::Count);
    static std::size_t index(InputAction action) { return static_cast<std::size_t>(action); }

    std::array<bool, kCount> held_{};
    std::array<bool, kCount> pressed_{};
};

} // namespace naru
