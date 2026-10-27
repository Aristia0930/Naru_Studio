#pragma once

#include "naru/events/Command.h"

#include <string>

namespace naru {

// 전역 스크립트 훅 이름. 프로젝트 설정의 전역 스크립트에 이 이름의 함수가 있으면 게임이 호출한다.
//   on_game_start(ctx)   게임을 시작할 때, 첫 장면에 들어가기 전에 한 번
//   on_scene_enter(ctx)  장면에 들어온 직후 (자동 실행 이벤트보다 먼저)
//   on_scene_exit(ctx)   다른 장면으로 가기 직전
//   on_update(ctx)       매 고정 틱 (기본 1/60초), 기본 조작보다 먼저
inline constexpr const char* kHookGameStart = "on_game_start";
inline constexpr const char* kHookSceneEnter = "on_scene_enter";
inline constexpr const char* kHookSceneExit = "on_scene_exit";
inline constexpr const char* kHookUpdate = "on_update";

enum class HookStatus {
    Ok,      // 하나 이상의 스크립트에서 실행함
    Missing, // 어느 스크립트에도 이 함수가 없음 (게임은 다시 부르지 않는다)
    Failed,  // 오류. 스크립트 쪽이 파일·행과 함께 로그를 남기고, 그 스크립트의 그 훅은 다시 부르지 않는다
};

// 게임 전체에 적용되는 스크립트 묶음. 엔진이 Python에 직접 의존하지 않도록 밖에서 구현해 넣는다.
class GameScripts {
public:
    virtual ~GameScripts() = default;
    // 게임 시작 때 한 번 읽는다 (파일의 최상위 코드가 이때 실행된다). 실패한 파일은 로그를 남기고 건너뛴다.
    // 오류(문법·변수 이름 겹침 등)의 수를 반환한다.
    virtual int load(CommandContext& ctx) = 0;
    // 읽어 둔 스크립트들에서 같은 이름의 함수를 등록 순서대로 호출한다.
    virtual HookStatus call(const std::string& hook, CommandContext& ctx) = 0;
};

} // namespace naru
