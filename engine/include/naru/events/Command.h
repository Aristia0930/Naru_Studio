#pragma once

#include <string>
#include <utility>

namespace naru {

class Logger;
class Variables;

enum class CommandStatus { Pending, Running, Waiting, Completed, Failed, Cancelled };

const char* toString(CommandStatus status);

// 명령이 실행 중 접근할 수 있는 세션 상태. 편집 문서가 아니라 실행용 복사본을 가리킨다.
struct CommandContext {
    Variables& variables;
    Logger& logger;
};

// 이벤트 명령 하나. start()에서 즉시 끝나면 Completed를, 시간이 필요하면 Waiting을 반환하고
// 이후 update()가 매 고정 틱마다 호출된다.
class Command {
public:
    explicit Command(std::string id) : id_(std::move(id)) {}
    virtual ~Command() = default;

    const std::string& id() const { return id_; }

    virtual CommandStatus start(CommandContext& ctx) = 0;
    virtual CommandStatus update(CommandContext&, double /*dt*/) { return CommandStatus::Completed; }
    virtual void cancel(CommandContext&) {}

    // Failed를 반환한 명령의 오류 설명.
    const std::string& error() const { return error_; }

protected:
    void setError(std::string message) { error_ = std::move(message); }

private:
    std::string id_;
    std::string error_;
};

} // namespace naru
