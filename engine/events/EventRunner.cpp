#include "naru/events/EventRunner.h"

#include "naru/core/Log.h"

#include <utility>

namespace naru {

const char* toString(CommandStatus status) {
    switch (status) {
    case CommandStatus::Pending: return "pending";
    case CommandStatus::Running: return "running";
    case CommandStatus::Waiting: return "waiting";
    case CommandStatus::Completed: return "completed";
    case CommandStatus::Failed: return "failed";
    case CommandStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

EventRunner::EventRunner(std::vector<std::unique_ptr<Command>> commands)
    : commands_(std::move(commands)) {}

bool EventRunner::finished() const {
    return status_ == CommandStatus::Completed || status_ == CommandStatus::Failed ||
           status_ == CommandStatus::Cancelled;
}

const Command* EventRunner::current() const {
    return index_ < commands_.size() ? commands_[index_].get() : nullptr;
}

void EventRunner::start(CommandContext& ctx, std::size_t startIndex) {
    index_ = startIndex;
    failedCommandId_.clear();
    failureMessage_.clear();
    status_ = CommandStatus::Running;
    runUntilBlocked(ctx);
}

void EventRunner::update(CommandContext& ctx, double dt) {
    if (status_ != CommandStatus::Waiting) {
        return;
    }
    advance(ctx, commands_[index_]->update(ctx, dt));
}

void EventRunner::cancel(CommandContext& ctx) {
    if (finished()) {
        return;
    }
    if (status_ == CommandStatus::Waiting) {
        commands_[index_]->cancel(ctx);
    }
    status_ = CommandStatus::Cancelled;
}

void EventRunner::advance(CommandContext& ctx, CommandStatus result) {
    switch (result) {
    case CommandStatus::Completed:
        ++index_;
        status_ = CommandStatus::Running;
        runUntilBlocked(ctx);
        break;
    case CommandStatus::Waiting:
    case CommandStatus::Running:
        status_ = CommandStatus::Waiting;
        break;
    case CommandStatus::Failed: {
        const Command& cmd = *commands_[index_];
        failedCommandId_ = cmd.id();
        failureMessage_ = cmd.error();
        status_ = CommandStatus::Failed;
        ctx.logger.error("명령 실패: " + failureMessage_, {"", 0, failedCommandId_});
        break;
    }
    case CommandStatus::Cancelled:
        status_ = CommandStatus::Cancelled;
        break;
    case CommandStatus::Pending:
        break;
    }
}

void EventRunner::runUntilBlocked(CommandContext& ctx) {
    // 즉시 끝나는 명령은 같은 틱 안에서 연속 실행하고, 대기가 필요한 명령에서 멈춘다.
    while (status_ == CommandStatus::Running) {
        if (index_ >= commands_.size()) {
            status_ = CommandStatus::Completed;
            return;
        }
        CommandStatus result = commands_[index_]->start(ctx);
        if (result == CommandStatus::Completed) {
            ++index_;
            continue;
        }
        advance(ctx, result);
    }
}

} // namespace naru
