#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/events/EventCompiler.h"
#include "naru/events/EventRunner.h"
#include "naru/project/Project.h"
#include "naru/runtime/GameScripts.h"
#include "naru/runtime/Input.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace naru {

enum class Facing { Down, Up, Left, Right };

const char* toKey(Facing facing);
std::optional<Facing> facingFromKey(const std::string& key);

// 게임 실행 상태. 편집 문서의 사본을 받아 장면에 들어가고, 실행 조건에 따라 객체 이벤트를 실행한다.
// 그리기·입력 장치·창은 다루지 않는다 (player와 테스트가 같은 코드를 쓴다).
//
// 기본 조작(코딩 없이 동작, RPG Maker 방식): 장면의 플레이어 객체를 방향 입력으로 움직이고,
// 지나갈 수 없는 객체와 장면 가장자리에 막힌다. 결정 입력으로 앞의 객체를 조사한다.
// 이벤트가 화면을 막고 있거나 대사가 떠 있으면 움직이지 않는다.
// 전역 스크립트는 setDefaultControl(false)로 기본 조작을 끄고 movePlayer()/interact()로 직접 조작하거나,
// 기본 조작은 둔 채 속도·키 배치를 바꾸고 새 동작을 덧붙일 수 있다.
//
// 실행 조건:
//   - 조사: 결정 키를 누르면 플레이어가 바라보는 쪽 바로 앞(또는 밟고 있는 통과 가능한) 객체
//   - 플레이어 접촉: 플레이어가 막는 객체에 부딪히거나 통과 가능한 객체와 겹칠 때.
//   - 이벤트 접촉: 위의 경우 + 객체가 움직여(moveObject) 플레이어에게 부딪히거나 겹칠 때.
//     둘 다 계속 밀거나 겹쳐 있는 동안에는 다시 실행하지 않는다.
//   - 자동 실행: 장면에 들어올 때 객체 순서대로 하나씩
//   - 병렬 처리: 장면에 들어올 때 모두 함께, 다른 이벤트를 막지 않음
// 조사·접촉·자동 실행 이벤트는 실행 중 플레이어 조작을 막으며 한 번에 하나씩 실행된다.
// 반복 조건(스위치·페이지)은 아직 없어 자동 실행·병렬 처리는 장면 진입 시 한 번 실행된다.
//
// 숨긴 객체(visible=false)는 막지도 막히지도 않고 조사·접촉 이벤트도 실행하지 않는다.
// 숨긴 플레이어는 아무것과도 닿지 않는다. 자동 실행·병렬 처리는 숨겨도 실행된다.
//
// 한 틱의 순서: on_update 훅 → 막는 이벤트 → 기본 조작 → 객체 이동 경로 → 접촉 판정 → 막는 이벤트
//   → 병렬 이벤트 → 예약된 장면 이동.
class GameWorld : public DialogueHost {
public:
    struct Message {
        std::string speaker;
        std::string text;
    };

    // 기본 이동 속도(픽셀/초)와 조사 거리(픽셀).
    static constexpr double kDefaultPlayerSpeed = 240.0;
    static constexpr double kActionReach = 16.0;

    GameWorld(Project project, ScriptRunner scripts, double fixedStep = 1.0 / 60.0);
    ~GameWorld() override;
    // 실행 중인 이벤트가 이 객체의 변수·로그를 참조하므로 복사·이동하지 않는다.
    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;

    // 전역 스크립트를 연결한다 (startGame 전에). 소유하지 않으며 GameWorld보다 오래 살아야 한다.
    void setGameScripts(GameScripts* scripts) { gameScripts_ = scripts; }
    // 전역 스크립트를 읽고 on_game_start를 부른 뒤 첫 장면에 들어간다. 장면이 없으면 false.
    bool startGame(const std::string& sceneId);
    // 장면에 들어갈 때 플레이어를 놓을 곳. 비어 있는 값은 장면 편집 상태 그대로 (방향은 아래).
    struct Arrival {
        std::optional<double> x, y;
        std::optional<Facing> facing;
    };
    // 장면에 들어간다. 이전 장면의 이벤트는 중지된다 (on_scene_exit → 전환·도착 위치 → on_scene_enter → 자동 실행).
    // 장면이 없으면 오류를 기록하고 false.
    bool enterScene(const std::string& sceneId, const Arrival& arrival = {});
    // 이번 틱이 끝난 뒤 장면을 바꾼다 (스크립트·이벤트 실행 도중에 장면이 사라지지 않도록).
    // 같은 틱에 여러 번 부르면 마지막 것이 쓰인다.
    void requestScene(const std::string& sceneId, const Arrival& arrival = {}) {
        pendingScene_ = sceneId;
        pendingArrival_ = arrival;
    }
    bool sceneChangePending() const { return pendingScene_.has_value(); }
    // 실제 경과 시간과 이번 프레임 입력을 받아 필요한 만큼 고정 틱을 진행한다.
    void update(double realSeconds, const InputState& input);

    const Project& project() const { return project_; }
    // 실행 중인 장면 (플레이어 위치 등 실행 중 바뀐 상태 포함). 들어간 장면이 없으면 nullptr.
    const Scene* scene() const { return scene_ ? &*scene_ : nullptr; }
    const std::optional<Message>& message() const { return message_; }
    Variables& variables() { return variables_; }
    Logger& logger() { return logger_; }
    // 플레이어 조작을 막는 이벤트(조사·접촉·자동 실행)나 대사가 진행 중인가.
    bool busy() const { return blockingEventActive() || message_.has_value(); }
    bool blockingEventActive() const;
    std::size_t runningEventCount() const;
    // 실패한 이벤트와 오류가 난 전역 스크립트 훅의 수.
    int failedEventCount() const { return failedEvents_; }
    std::uint64_t tickCount() const { return ticks_; }

    // ---- 입력 (현재 틱 기준. "새로 누름"은 프레임의 첫 틱에서만 참)
    InputMap& inputMap() { return inputMap_; }
    const InputMap& inputMap() const { return inputMap_; }
    const InputState& input() const { return tickInput_; }

    // ---- 플레이어
    // 현재 장면의 플레이어 객체. 지정하지 않았으면 nullptr.
    const SceneObject* player() const;
    Facing facing() const { return facing_; }
    void setFacing(Facing facing) { facing_ = facing; }
    double playerSpeed() const { return playerSpeed_; }
    void setPlayerSpeed(double pixelsPerSecond) { playerSpeed_ = pixelsPerSecond; }
    bool defaultControl() const { return defaultControl_; }
    void setDefaultControl(bool enabled) { defaultControl_ = enabled; }
    // 충돌을 따져 dx, dy 픽셀만큼 움직인다 (접촉 이벤트도 판정). 플레이어가 없으면 false.
    bool movePlayer(double dx, double dy);
    // 충돌 없이 위치를 옮긴다.
    bool setPlayerPosition(double x, double y);
    // 결정 키를 누른 것처럼 앞의 객체를 조사한다. 이벤트를 시작했으면 true.
    bool interact();

    // ---- 장면 객체 (실행 중 상태. 장면에 다시 들어오면 편집 상태로 돌아간다)
    const SceneObject* object(const std::string& id) const;
    // 충돌을 따져 움직인다 (규칙은 플레이어와 같다: 통과 가능하지 않은 것끼리 막는다).
    // 플레이어 객체면 movePlayer와 같다. 객체가 없으면 false.
    bool moveObject(const std::string& id, double dx, double dy);
    // 충돌 없이 옮긴다.
    bool setObjectPosition(const std::string& id, double x, double y);
    bool setObjectPassable(const std::string& id, bool passable);
    bool setObjectColor(const std::string& id, const std::string& color);
    bool setObjectVisible(const std::string& id, bool visible);
    // "@self"(selfId)·"@player"·객체 ID를 실제 객체 ID로 바꾼다. 장면에 없으면 빈 문자열.
    std::string resolveTarget(const std::string& target, const std::string& selfId) const;

    // ---- 이동 경로 (이벤트 "객체 이동" 명령). 매 틱 speed(픽셀/초)로 moveObject와 같은 규칙으로 움직이며,
    // 막히거나 장면 가장자리에 닿으면 그 자리에서 끝난다. speed <= 0이면 바로 옮긴다.
    // 같은 객체에 새로 시작하면 앞의 이동을 대신한다. 장면에 다시 들어오면 모두 멈춘다. 객체가 없으면 false.
    bool startObjectMove(const std::string& id, double dx, double dy, double speed);
    bool objectMoving(const std::string& id) const;
    void stopObjectMove(const std::string& id);

    // DialogueHost
    void showMessage(const std::string& speaker, const std::string& text) override;
    bool messageClosed() const override { return !message_.has_value(); }
    void closeMessage() override { message_.reset(); }

private:
    struct RunningEvent;

    std::unique_ptr<RunningEvent> makeEvent(const SceneObject& object);
    // 한 틱 진행. 끝나면 true.
    bool tickEvent(RunningEvent& event, double dt);
    // 막는 이벤트를 진행한다. 즉시 끝나는 이벤트는 같은 틱 안에서 이어서 실행한다.
    void runBlocking(double dt);
    void stopAll();
    void callHook(const char* hook);

    SceneObject* playerObject();
    void controlPlayer(bool actionPressed, double dt);
    // 가로 → 세로 순서로 이동하고 막은 객체 ID들을 돌려준다.
    std::vector<std::string> moveAxes(SceneObject& mover, double dx, double dy);
    void moveAxis(SceneObject& mover, double dx, double dy, std::vector<std::string>& blockers);
    // 이번 틱의 접촉을 정리하고, 새로 닿은 객체의 접촉 이벤트를 시작한다.
    void finishContacts();
    void queueBlocking(const SceneObject& object);
    void advanceRoutes(double dt);

    Project project_;
    ScriptRunner scripts_;
    double fixedStep_;
    Variables variables_;
    Logger logger_;
    std::optional<Scene> scene_;
    std::optional<Message> message_;

    GameScripts* gameScripts_ = nullptr;
    // 전역 스크립트 훅이 쓰는 실행 컨텍스트 (이벤트와 같은 변수·로그·데이터베이스).
    CommandContext hookCtx_;
    // 어느 전역 스크립트에도 없는 훅은 다시 부르지 않는다.
    std::set<std::string> disabledHooks_;
    std::optional<std::string> pendingScene_;
    Arrival pendingArrival_;

    std::unique_ptr<RunningEvent> blocking_;
    std::deque<std::string> blockingQueue_;
    std::vector<std::unique_ptr<RunningEvent>> parallel_;

    struct Route {
        std::string objectId;
        double dirX, dirY; // 단위 방향
        double remaining;  // 남은 거리 (픽셀)
        double speed;
    };
    std::vector<Route> routes_;

    InputMap inputMap_ = InputMap::defaults();
    InputState tickInput_;
    double playerSpeed_ = kDefaultPlayerSpeed;
    bool defaultControl_ = true;
    Facing facing_ = Facing::Down;
    // 지난 틱에 플레이어와 닿아 있던 객체. 새로 닿은 객체만 접촉 이벤트를 실행한다.
    std::set<std::string> contacts_;
    std::set<std::string> tickContacts_;
    // 움직여서 플레이어에 부딪힌 객체 (지난 틱 / 이번 틱). 이벤트 접촉 판정용.
    std::set<std::string> objectBumps_;
    std::set<std::string> tickObjectBumps_;
    bool playerMovedThisTick_ = false;

    double accumulator_ = 0.0;
    std::uint64_t ticks_ = 0;
    int failedEvents_ = 0;
};

} // namespace naru
