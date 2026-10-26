#pragma once

#include "naru/core/Value.h"
#include "naru/events/Command.h"

#include <functional>
#include <string>

namespace naru {

class LogCommand : public Command {
public:
    LogCommand(std::string id, std::string message);
    CommandStatus start(CommandContext& ctx) override;

private:
    std::string message_;
};

class WaitCommand : public Command {
public:
    WaitCommand(std::string id, double seconds);
    CommandStatus start(CommandContext& ctx) override;
    CommandStatus update(CommandContext& ctx, double dt) override;
    void cancel(CommandContext& ctx) override;

    double remaining() const { return remaining_; }

private:
    double seconds_;
    double remaining_ = 0.0;
};

class SetVariableCommand : public Command {
public:
    SetVariableCommand(std::string id, std::string name, Value value);
    CommandStatus start(CommandContext& ctx) override;

private:
    std::string name_;
    Value value_;
};

// 외부 기능(예: Python 스크립트 호출)을 명령으로 연결한다. 빈 문자열을 반환하면 성공, 아니면 그 내용이 오류다.
class CallCommand : public Command {
public:
    using Function = std::function<std::string(CommandContext&)>;

    CallCommand(std::string id, Function function);
    CommandStatus start(CommandContext& ctx) override;

private:
    Function function_;
};

} // namespace naru
