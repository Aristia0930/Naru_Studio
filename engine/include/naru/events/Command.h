#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace naru {

class Logger;
class Variables;

enum class CommandStatus { Pending, Running, Waiting, Completed, Failed, Cancelled };

const char* toString(CommandStatus status);

// 대사 창을 보여 주는 쪽 (에디터 미리보기, 이후 게임 화면). 플레이어가 넘길 때까지 대사 명령이 기다린다.
class DialogueHost {
public:
    virtual ~DialogueHost() = default;
    virtual void showMessage(const std::string& speaker, const std::string& text) = 0;
    virtual bool messageClosed() const = 0;
    virtual void closeMessage() = 0;
};

// 명령이 실행 중 접근할 수 있는 세션 상태. 편집 문서가 아니라 실행용 복사본을 가리킨다.
struct CommandContext {
    Variables& variables;
    Logger& logger;
    DialogueHost* dialogue = nullptr;
    // 명령이 Completed와 함께 설정하면 다음 명령 대신 이 위치로 이동한다 (조건 분기).
    std::optional<std::size_t> jump{};
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
