#pragma once

#include "naru/events/Command.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace naru {

// 명령 목록을 순서대로 실행한다. 실패하면 다음 명령으로 넘어가지 않고 멈춘다.
class EventRunner {
public:
    EventRunner() = default;
    explicit EventRunner(std::vector<std::unique_ptr<Command>> commands);

    // startIndex부터 실행한다 (선택한 명령부터 테스트하는 기능).
    void start(CommandContext& ctx, std::size_t startIndex = 0);
    void update(CommandContext& ctx, double dt);
    void cancel(CommandContext& ctx);

    CommandStatus status() const { return status_; }
    bool finished() const;
    std::size_t currentIndex() const { return index_; }
    std::size_t size() const { return commands_.size(); }
    const Command* current() const;

    // 실패한 명령의 ID와 오류.
    const std::string& failedCommandId() const { return failedCommandId_; }
    const std::string& failureMessage() const { return failureMessage_; }

    static constexpr std::size_t kImmediateCommandBudget = 10000;

private:
    void moveNext(CommandContext& ctx);
    void advance(CommandContext& ctx, CommandStatus result);
    void runUntilBlocked(CommandContext& ctx);

    std::vector<std::unique_ptr<Command>> commands_;
    std::size_t index_ = 0;
    CommandStatus status_ = CommandStatus::Pending;
    std::string failedCommandId_;
    std::string failureMessage_;
};

} // namespace naru
