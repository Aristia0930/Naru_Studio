#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace naru {

class Logger;
class Variables;
class GameWorld;
struct Database;

enum class CommandStatus { Pending, Running, Waiting, Completed, Failed, Cancelled };

const char* toString(CommandStatus status);

// 대사 창을 보여 주는 쪽 (에디터 미리보기, 이후 게임 화면). 플레이어가 넘길 때까지 대사 명령이 기다린다.
struct ChoiceRequest {
    std::string speaker, text;
    std::vector<std::string> options;
    int defaultChoice = 0;
    int cancelChoice = -1;
    std::optional<std::size_t> selected;
    bool cancelled = false;
};

class DialogueHost {
public:
    virtual ~DialogueHost() = default;
    virtual void showMessage(const std::string& speaker, const std::string& text) = 0;
    virtual bool messageClosed() const = 0;
    virtual void closeMessage() = 0;
    // 요청은 실행 명령별로 소유한다. 다른 병렬 이벤트의 선택 결과를 가져가지 않는다.
    virtual bool showChoices(const std::shared_ptr<ChoiceRequest>& request) {
        if (!messageClosed()) return false;
        choices_ = request;
        showMessage(request->speaker, request->text);
        return true;
    }
    const std::shared_ptr<ChoiceRequest>& choices() const { return choices_; }
    bool selectChoice(std::size_t index) {
        if (!choices_ || index >= choices_->options.size()) return false;
        choices_->selected = index;
        choices_.reset();
        closeMessage();
        return true;
    }
    void closeChoices(const std::shared_ptr<ChoiceRequest>& request) {
        if (choices_ == request) {
            clearChoices();
            closeMessage();
        }
    }
protected:
    void clearChoices() {
        if (choices_) choices_->cancelled = true;
        choices_.reset();
    }
private:
    std::shared_ptr<ChoiceRequest> choices_;
};

// 명령이 실행 중 접근할 수 있는 세션 상태. 편집 문서가 아니라 실행용 복사본을 가리킨다.
struct CommandContext {
    Variables& variables;
    Logger& logger;
    DialogueHost* dialogue = nullptr;
    // 명령이 Completed와 함께 설정하면 다음 명령 대신 이 위치로 이동한다 (조건 분기).
    std::optional<std::size_t> jump{};
    // 읽기 전용 데이터베이스 (실행 중에는 바꾸지 않는다). 없으면 데이터 명령이 실패한다.
    const Database* database = nullptr;
    // 게임 실행 중이면 실행 상태(플레이어·입력·장면). 에디터 테스트 실행이나 단독 스크립트 실행에서는 nullptr.
    GameWorld* game = nullptr;
    // 이 이벤트가 붙은 객체 ID ("@self" 대상). 객체 이벤트가 아니면 비어 있다.
    std::string selfObjectId{};
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
