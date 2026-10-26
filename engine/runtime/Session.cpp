#include "naru/runtime/Session.h"

#include <utility>

namespace naru {

namespace {
// 긴 정지(디버거 중단 등) 뒤에 틱이 폭주하지 않도록 한 번에 처리할 최대 틱 수.
constexpr int kMaxTicksPerAdvance = 8;
} // namespace

const char* toString(SessionState state) {
    switch (state) {
    case SessionState::Idle: return "idle";
    case SessionState::Running: return "running";
    case SessionState::Stopped: return "stopped";
    case SessionState::Completed: return "completed";
    case SessionState::Failed: return "failed";
    }
    return "unknown";
}

Session::Session(const Variables& initialVariables, double fixedStep)
    : variables_(initialVariables), ctx_{variables_, logger_}, fixedStep_(fixedStep) {}

void Session::start(std::vector<std::unique_ptr<Command>> commands, std::size_t startIndex) {
    runner_ = EventRunner(std::move(commands));
    accumulator_ = 0.0;
    ticks_ = 0;
    state_ = SessionState::Running;
    runner_.start(ctx_, startIndex);
    syncStateFromRunner();
}

int Session::advance(double realSeconds) {
    if (state_ != SessionState::Running) {
        return 0;
    }
    accumulator_ += realSeconds;
    int ran = 0;
    while (accumulator_ >= fixedStep_ && ran < kMaxTicksPerAdvance) {
        accumulator_ -= fixedStep_;
        ++ticks_;
        ++ran;
        runner_.update(ctx_, fixedStep_);
        syncStateFromRunner();
        if (state_ != SessionState::Running) {
            break;
        }
    }
    if (ran == kMaxTicksPerAdvance) {
        accumulator_ = 0.0;
    }
    return ran;
}

void Session::stop() {
    if (state_ != SessionState::Running) {
        return;
    }
    runner_.cancel(ctx_);
    state_ = SessionState::Stopped;
    logger_.info("실행 중지");
}

void Session::syncStateFromRunner() {
    switch (runner_.status()) {
    case CommandStatus::Completed: state_ = SessionState::Completed; break;
    case CommandStatus::Failed: state_ = SessionState::Failed; break;
    case CommandStatus::Cancelled: state_ = SessionState::Stopped; break;
    default: break;
    }
}

} // namespace naru
