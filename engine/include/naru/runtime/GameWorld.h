#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/events/EventCompiler.h"
#include "naru/events/EventRunner.h"
#include "naru/project/Project.h"
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

// 게임 실행 상태. 편집 문서의 사본을 받아 장면에 들어가고, 실행 조건에 따라 객체 이벤트를 실행한다.
// 그리기·입력 장치·창은 다루지 않는다 (player와 테스트가 같은 코드를 쓴다).
//
// 기본 조작(코딩 없이 동작, RPG Maker 방식): 장면의 플레이어 객체를 방향 입력으로 움직이고,
// 지나갈 수 없는 객체와 장면 가장자리에 막힌다. 이벤트가 화면을 막고 있거나 대사가 떠 있으면 움직이지 않는다.
// 스크립트로 기본 조작을 끄거나 덧붙이는 기능은 전역 스크립트 훅과 함께 추가한다 (ROADMAP P2).
//
// 실행 조건:
//   - 조사: 결정 키를 누르면 플레이어가 바라보는 쪽 바로 앞(또는 밟고 있는 통과 가능한) 객체
//   - 플레이어 접촉 / 이벤트 접촉: 막는 객체에 부딪히거나 통과 가능한 객체 위로 들어설 때.
//     계속 밀거나 서 있는 동안에는 다시 실행하지 않는다. (이벤트 접촉의 "객체가 움직여 닿을 때"는 객체 이동이 생기면 추가)
//   - 자동 실행: 장면에 들어올 때 객체 순서대로 하나씩
//   - 병렬 처리: 장면에 들어올 때 모두 함께, 다른 이벤트를 막지 않음
// 조사·접촉·자동 실행 이벤트는 실행 중 플레이어 조작을 막으며 한 번에 하나씩 실행된다.
// 반복 조건(스위치·페이지)은 아직 없어 자동 실행·병렬 처리는 장면 진입 시 한 번 실행된다.
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

    // 장면에 들어간다. 이전 장면의 이벤트는 중지된다. 장면이 없으면 오류를 기록하고 false.
    bool enterScene(const std::string& sceneId);
    // 실제 경과 시간과 이번 프레임 입력을 받아 필요한 만큼 고정 틱을 진행한다.
    void update(double realSeconds, const InputState& input);

    const Project& project() const { return project_; }
    // 실행 중인 장면 (플레이어 위치 등 실행 중 바뀐 상태 포함). 들어간 장면이 없으면 nullptr.
    const Scene* scene() const { return scene_ ? &*scene_ : nullptr; }
    // 현재 장면의 플레이어 객체. 지정하지 않았으면 nullptr.
    const SceneObject* player() const;
    Facing facing() const { return facing_; }
    const std::optional<Message>& message() const { return message_; }
    Variables& variables() { return variables_; }
    Logger& logger() { return logger_; }
    // 플레이어 조작을 막는 이벤트(조사·접촉·자동 실행)가 실행 중이거나 기다리는 중인가.
    bool blockingEventActive() const;
    std::size_t runningEventCount() const;
    int failedEventCount() const { return failedEvents_; }
    std::uint64_t tickCount() const { return ticks_; }
    double playerSpeed() const { return playerSpeed_; }
    void setPlayerSpeed(double pixelsPerSecond) { playerSpeed_ = pixelsPerSecond; }

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

    SceneObject* playerObject();
    void controlPlayer(const InputState& input, bool actionPressed, double dt);
    // 한 축으로 이동하고 막힌 객체를 contacts에 넣는다.
    void movePlayer(SceneObject& player, double dx, double dy, std::set<std::string>& contacts);
    void tryAction(const SceneObject& player);
    void queueBlocking(const SceneObject& object);

    Project project_;
    ScriptRunner scripts_;
    double fixedStep_;
    Variables variables_;
    Logger logger_;
    std::optional<Scene> scene_;
    std::optional<Message> message_;

    std::unique_ptr<RunningEvent> blocking_;
    std::deque<std::string> blockingQueue_;
    std::vector<std::unique_ptr<RunningEvent>> parallel_;

    double playerSpeed_ = kDefaultPlayerSpeed;
    Facing facing_ = Facing::Down;
    // 지난 틱에 플레이어와 닿아 있던 객체. 새로 닿은 객체만 접촉 이벤트를 실행한다.
    std::set<std::string> contacts_;

    double accumulator_ = 0.0;
    std::uint64_t ticks_ = 0;
    int failedEvents_ = 0;
};

} // namespace naru
