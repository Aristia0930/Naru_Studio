#include "naru/events/BuiltinCommands.h"

#include "naru/core/Log.h"

#include <utility>

namespace naru {

LogCommand::LogCommand(std::string id, std::string message)
    : Command(std::move(id)), message_(std::move(message)) {}

CommandStatus LogCommand::start(CommandContext& ctx) {
    ctx.logger.info(message_, {"", 0, id()});
    return CommandStatus::Completed;
}

WaitCommand::WaitCommand(std::string id, double seconds)
    : Command(std::move(id)), seconds_(seconds) {}

CommandStatus WaitCommand::start(CommandContext&) {
    remaining_ = seconds_;
    return remaining_ > 0.0 ? CommandStatus::Waiting : CommandStatus::Completed;
}

CommandStatus WaitCommand::update(CommandContext&, double dt) {
    remaining_ -= dt;
    return remaining_ > 0.0 ? CommandStatus::Waiting : CommandStatus::Completed;
}

void WaitCommand::cancel(CommandContext&) {
    remaining_ = 0.0;
}

SetVariableCommand::SetVariableCommand(std::string id, std::string name, Value value)
    : Command(std::move(id)), name_(std::move(name)), value_(std::move(value)) {}

CommandStatus SetVariableCommand::start(CommandContext& ctx) {
    ctx.variables.set(name_, value_);
    return CommandStatus::Completed;
}

CallCommand::CallCommand(std::string id, Function function)
    : Command(std::move(id)), function_(std::move(function)) {}

CommandStatus CallCommand::start(CommandContext& ctx) {
    std::string error = function_(ctx);
    if (!error.empty()) {
        setError(std::move(error));
        return CommandStatus::Failed;
    }
    return CommandStatus::Completed;
}

} // namespace naru
