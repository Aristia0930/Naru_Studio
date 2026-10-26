#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/events/EventRunner.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace naru {

enum class SessionState { Idle, Running, Stopped, Completed, Failed };

const char* toString(SessionState state);

// 실행 세션. 편집 문서의 변수를 복사해서 시작하므로, 실행 중 변경이 원본에 남지 않는다.
// 게임 로직은 고정 간격(기본 1/60초)으로 진행하고, 표시 쪽은 advance()에 실제 경과 시간을 넘긴다.
class Session {
public:
    explicit Session(const Variables& initialVariables, double fixedStep = 1.0 / 60.0);
    // ctx_가 자기 멤버를 참조하므로 복사·이동하지 않는다.
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void start(std::vector<std::unique_ptr<Command>> commands, std::size_t startIndex = 0);
    // 실제 경과 시간을 받아 필요한 만큼 고정 틱을 실행한다. 실행한 틱 수를 반환한다.
    int advance(double realSeconds);
    void stop();

    SessionState state() const { return state_; }
    const Variables& variables() const { return variables_; }
    Logger& logger() { return logger_; }
    const EventRunner& runner() const { return runner_; }
    std::uint64_t tickCount() const { return ticks_; }
    double fixedStep() const { return fixedStep_; }

private:
    void syncStateFromRunner();

    Variables variables_;
    Logger logger_;
    EventRunner runner_;
    CommandContext ctx_;
    SessionState state_ = SessionState::Idle;
    double fixedStep_;
    double accumulator_ = 0.0;
    std::uint64_t ticks_ = 0;
};

} // namespace naru
