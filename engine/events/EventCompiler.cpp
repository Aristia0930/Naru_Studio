#include "naru/events/EventCompiler.h"

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/data/Database.h"
#include "naru/events/BuiltinCommands.h"
#include "naru/runtime/GameWorld.h"

#include <cstdio>
#include <utility>

namespace naru {

namespace {

bool isNumber(const Value& v) {
    return std::holds_alternative<std::int64_t>(v) || std::holds_alternative<double>(v);
}

double asDouble(const Value& v) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return static_cast<double>(*i);
    }
    return std::get<double>(v);
}

const char* typeName(const Value& v) {
    switch (v.index()) {
    case 0: return "없음";
    case 1: return "참/거짓";
    case 2: return "정수";
    case 3: return "실수";
    case 4: return "문자열";
    }
    return "?";
}

class SayCommand : public Command {
public:
    SayCommand(std::string id, std::string speaker, std::string text)
        : Command(std::move(id)), speaker_(std::move(speaker)), text_(std::move(text)) {}

    CommandStatus start(CommandContext& ctx) override {
        ctx.logger.info(speaker_.empty() ? text_ : "[" + speaker_ + "] " + text_, {"", 0, id()});
        if (ctx.dialogue == nullptr) {
            return CommandStatus::Completed;
        }
        ctx.dialogue->showMessage(speaker_, text_);
        return CommandStatus::Waiting;
    }

    CommandStatus update(CommandContext& ctx, double) override {
        return ctx.dialogue->messageClosed() ? CommandStatus::Completed : CommandStatus::Waiting;
    }

    void cancel(CommandContext& ctx) override {
        if (ctx.dialogue) {
            ctx.dialogue->closeMessage();
        }
    }

private:
    std::string speaker_;
    std::string text_;
};

class VariableCommand : public Command {
public:
    VariableCommand(std::string id, std::string name, VariableOp op, Value value)
        : Command(std::move(id)), name_(std::move(name)), op_(op), value_(std::move(value)) {}

    CommandStatus start(CommandContext& ctx) override {
        if (name_.empty()) {
            setError("변수 이름이 비어 있습니다");
            return CommandStatus::Failed;
        }
        std::string error;
        auto result = applyVariableOp(ctx.variables.get(name_).value_or(Value{}), op_, value_, error);
        if (!result) {
            setError("변수 '" + name_ + "': " + error);
            return CommandStatus::Failed;
        }
        ctx.variables.set(name_, *result);
        return CommandStatus::Completed;
    }

private:
    std::string name_;
    VariableOp op_;
    Value value_;
};

// 조건이 거짓이면 elseTarget(아니라면 분기 시작 또는 분기 끝)으로 이동한다.
class BranchCommand : public Command {
public:
    BranchCommand(std::string id, std::string name, CompareOp compare, Value value)
        : Command(std::move(id)), name_(std::move(name)), compare_(compare), value_(std::move(value)) {}

    void setElseTarget(std::size_t target) { elseTarget_ = target; }

    CommandStatus start(CommandContext& ctx) override {
        std::string error;
        // 아직 없는 변수를 숫자와 비교하면 0으로 본다 (변수 더하기와 같은 규칙).
        Value current = ctx.variables.get(name_).value_or(
            isNumber(value_) ? Value{std::int64_t{0}} : Value{});
        bool result = compareValues(current, compare_, value_, error);
        if (!error.empty()) {
            setError("조건 '" + name_ + "': " + error);
            return CommandStatus::Failed;
        }
        if (!result) {
            ctx.jump = elseTarget_;
        }
        return CommandStatus::Completed;
    }

private:
    std::string name_;
    CompareOp compare_;
    Value value_;
    std::size_t elseTarget_ = 0;
};

// 데이터베이스 항목의 속성 값을 변수에 넣는다.
class GetDataCommand : public Command {
public:
    GetDataCommand(std::string id, const EventCommand& c)
        : Command(std::move(id)), type_(c.dataType), record_(c.record), field_(c.field), variable_(c.variable) {}

    CommandStatus start(CommandContext& ctx) override {
        if (variable_.empty()) {
            setError("결과를 넣을 변수 이름이 비어 있습니다");
            return CommandStatus::Failed;
        }
        if (!ctx.database) {
            setError("데이터베이스가 연결되지 않았습니다");
            return CommandStatus::Failed;
        }
        const DataType* type = ctx.database->findType(type_);
        if (!type) {
            setError("데이터 종류가 없습니다: " + type_);
            return CommandStatus::Failed;
        }
        const DataRecord* record = type->findRecord(record_);
        if (!record) {
            setError(type->name + "에 항목이 없습니다: " + record_);
            return CommandStatus::Failed;
        }
        if (field_ == "id") {
            ctx.variables.set(variable_, record->id);
        } else if (field_ == "name") {
            ctx.variables.set(variable_, record->name);
        } else if (const FieldDef* f = type->findField(field_)) {
            ctx.variables.set(variable_, type->valueOf(*record, *f));
        } else {
            setError(type->name + "에 속성이 없습니다: " + field_);
            return CommandStatus::Failed;
        }
        return CommandStatus::Completed;
    }

private:
    std::string type_;
    std::string record_;
    std::string field_;
    std::string variable_;
};

std::string number(double v) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%g", v);
    return buffer;
}

// 로그에 쓰는 대상 이름.
std::string targetLabel(const std::string& target) {
    if (target == kTargetSelf) return "이 객체";
    if (target == kTargetPlayer) return "플레이어";
    return target;
}

// 객체를 정한 방향·거리만큼 움직인다 (GameWorld의 이동 경로). 게임 밖(에디터 테스트 실행)에서는 로그만 남긴다.
class MoveObjectCommand : public Command {
public:
    MoveObjectCommand(std::string id, const EventCommand& c)
        : Command(std::move(id)), target_(c.target), direction_(c.direction), distance_(c.distance),
          speed_(c.speed), wait_(c.waitForEnd) {}

    CommandStatus start(CommandContext& ctx) override {
        movingId_.clear();
        auto facing = facingFromKey(direction_);
        if (!facing) {
            setError("알 수 없는 이동 방향: " + direction_);
            return CommandStatus::Failed;
        }
        if (distance_ < 0.0) {
            setError("이동 거리는 0 이상이어야 합니다");
            return CommandStatus::Failed;
        }
        if (!ctx.game) {
            ctx.logger.info("객체 이동 (게임 실행 중에만 움직입니다): " + targetLabel(target_) + " → " +
                                directionLabel(*facing) + " " + number(distance_) + "px",
                            {"", 0, id()});
            return CommandStatus::Completed;
        }
        std::string objectId = ctx.game->resolveTarget(target_, ctx.selfObjectId);
        if (objectId.empty()) {
            setError("이동할 객체가 장면에 없습니다: " + targetLabel(target_));
            return CommandStatus::Failed;
        }
        double dx = *facing == Facing::Left ? -1.0 : *facing == Facing::Right ? 1.0 : 0.0;
        double dy = *facing == Facing::Up ? -1.0 : *facing == Facing::Down ? 1.0 : 0.0;
        ctx.game->startObjectMove(objectId, dx * distance_, dy * distance_, speed_);
        if (!wait_ || !ctx.game->objectMoving(objectId)) {
            return CommandStatus::Completed;
        }
        movingId_ = objectId;
        return CommandStatus::Waiting;
    }

    CommandStatus update(CommandContext& ctx, double) override {
        return ctx.game && ctx.game->objectMoving(movingId_) ? CommandStatus::Waiting : CommandStatus::Completed;
    }

    // 이벤트가 중지되면 기다리던 이동도 멈춘다 (기다리지 않는 이동은 이벤트와 상관없이 계속된다).
    void cancel(CommandContext& ctx) override {
        if (ctx.game && !movingId_.empty()) {
            ctx.game->stopObjectMove(movingId_);
        }
    }

    static std::string directionLabel(Facing f) {
        switch (f) {
        case Facing::Up: return "위";
        case Facing::Down: return "아래";
        case Facing::Left: return "왼쪽";
        case Facing::Right: return "오른쪽";
        }
        return "";
    }

private:
    std::string target_;
    std::string direction_;
    double distance_;
    double speed_;
    bool wait_;
    std::string movingId_;
};

class SetVisibleCommand : public Command {
public:
    SetVisibleCommand(std::string id, std::string target, bool show)
        : Command(std::move(id)), target_(std::move(target)), show_(show) {}

    CommandStatus start(CommandContext& ctx) override {
        if (!ctx.game) {
            ctx.logger.info(std::string(show_ ? "보이기" : "숨기기") + " (게임 실행 중에만 바뀝니다): " +
                                targetLabel(target_),
                            {"", 0, id()});
            return CommandStatus::Completed;
        }
        std::string objectId = ctx.game->resolveTarget(target_, ctx.selfObjectId);
        if (objectId.empty()) {
            setError(std::string(show_ ? "보일" : "숨길") + " 객체가 장면에 없습니다: " + targetLabel(target_));
            return CommandStatus::Failed;
        }
        ctx.game->setObjectVisible(objectId, show_);
        return CommandStatus::Completed;
    }

private:
    std::string target_;
    bool show_;
};

// 장면을 바꾼다 (이번 틱이 끝난 뒤). 장면이 바뀌면 이 이벤트는 중지되므로 뒤의 명령은 실행되지 않는다.
// 게임 밖(에디터 테스트 실행)에서는 로그만 남긴다.
class ChangeSceneCommand : public Command {
public:
    ChangeSceneCommand(std::string id, const EventCommand& c)
        : Command(std::move(id)), scene_(c.scene), direction_(c.direction), setPosition_(c.setPosition), x_(c.x),
          y_(c.y) {}

    CommandStatus start(CommandContext& ctx) override {
        std::optional<Facing> facing;
        if (!direction_.empty()) {
            facing = facingFromKey(direction_);
            if (!facing) {
                setError("알 수 없는 방향: " + direction_);
                return CommandStatus::Failed;
            }
        }
        if (scene_.empty()) {
            setError("이동할 장면을 지정하지 않았습니다");
            return CommandStatus::Failed;
        }
        if (!ctx.game) {
            std::string where = setPosition_ ? " (" + number(x_) + ", " + number(y_) + ")" : std::string();
            ctx.logger.info("장면 이동 (게임 실행 중에만 바뀝니다): " + scene_ + where, {"", 0, id()});
            return CommandStatus::Completed;
        }
        if (!ctx.game->project().findScene(scene_)) {
            setError("장면이 없습니다: " + scene_);
            return CommandStatus::Failed;
        }
        GameWorld::Arrival arrival;
        if (setPosition_) {
            arrival.x = x_;
            arrival.y = y_;
        }
        // 방향을 정하지 않으면 지금 바라보는 방향 그대로
        arrival.facing = facing.value_or(ctx.game->facing());
        ctx.game->requestScene(scene_, arrival);
        return CommandStatus::Waiting;
    }

    // 장면이 바뀌면서 취소될 때까지 기다린다.
    CommandStatus update(CommandContext&, double) override { return CommandStatus::Waiting; }

private:
    std::string scene_;
    std::string direction_;
    bool setPosition_;
    double x_;
    double y_;
};

// "그렇다면" 분기 끝에서 "아니라면" 분기를 건너뛴다.
class JumpCommand : public Command {
public:
    using Command::Command;
    void setTarget(std::size_t target) { target_ = target; }

    CommandStatus start(CommandContext& ctx) override {
        ctx.jump = target_;
        return CommandStatus::Completed;
    }

private:
    std::size_t target_ = 0;
};

void emit(const std::vector<EventCommand>& list, const std::string& prefix,
          std::vector<std::unique_ptr<Command>>& out, const ScriptRunner& runScript) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        const EventCommand& c = list[i];
        std::string id = commandId(prefix, i);
        switch (c.type) {
        case EventCommandType::Say:
            out.push_back(std::make_unique<SayCommand>(id, c.speaker, c.text));
            break;
        case EventCommandType::Wait:
            out.push_back(std::make_unique<WaitCommand>(id, c.seconds));
            break;
        case EventCommandType::SetVariable:
            out.push_back(std::make_unique<VariableCommand>(id, c.variable, c.op, c.value));
            break;
        case EventCommandType::Log:
            out.push_back(std::make_unique<LogCommand>(id, c.text));
            break;
        case EventCommandType::CallScript: {
            std::string script = c.script;
            std::string function = c.function.empty() ? std::string("run") : c.function;
            std::string resultVariable = c.variable;
            out.push_back(std::make_unique<CallCommand>(
                id, [runScript, script, function, resultVariable](CommandContext& ctx) -> std::string {
                    if (script.empty()) {
                        return "호출할 스크립트를 지정하지 않았습니다";
                    }
                    if (!runScript) {
                        return "스크립트 실행기가 연결되지 않았습니다";
                    }
                    ScriptCallResult result = runScript(script, function, ctx);
                    if (!result.error.empty()) {
                        return result.error;
                    }
                    if (!resultVariable.empty()) {
                        ctx.variables.set(resultVariable, std::move(result.returned));
                    }
                    return {};
                }));
            break;
        }
        case EventCommandType::IfVariable: {
            auto branch = std::make_unique<BranchCommand>(id, c.variable, c.compare, c.value);
            BranchCommand* branchPtr = branch.get();
            out.push_back(std::move(branch));
            emit(c.thenCommands, thenPrefix(id), out, runScript);
            if (c.elseCommands.empty()) {
                branchPtr->setElseTarget(out.size());
            } else {
                auto jump = std::make_unique<JumpCommand>(id);
                JumpCommand* jumpPtr = jump.get();
                out.push_back(std::move(jump));
                branchPtr->setElseTarget(out.size());
                emit(c.elseCommands, elsePrefix(id), out, runScript);
                jumpPtr->setTarget(out.size());
            }
            break;
        }
        case EventCommandType::GetData:
            out.push_back(std::make_unique<GetDataCommand>(id, c));
            break;
        case EventCommandType::MoveObject:
            out.push_back(std::make_unique<MoveObjectCommand>(id, c));
            break;
        case EventCommandType::SetVisible:
            out.push_back(std::make_unique<SetVisibleCommand>(id, c.target, c.show));
            break;
        case EventCommandType::ChangeScene:
            out.push_back(std::make_unique<ChangeSceneCommand>(id, c));
            break;
        case EventCommandType::Comment:
            break; // 실행하지 않는다
        }
    }
}

} // namespace

std::vector<std::unique_ptr<Command>> compileEvent(const std::vector<EventCommand>& commands,
                                                   const ScriptRunner& runScript) {
    std::vector<std::unique_ptr<Command>> out;
    emit(commands, std::string(), out, runScript);
    return out;
}

bool compareValues(const Value& left, CompareOp op, const Value& right, std::string& error) {
    error.clear();
    if (isNumber(left) && isNumber(right)) {
        double a = asDouble(left);
        double b = asDouble(right);
        switch (op) {
        case CompareOp::Equal: return a == b;
        case CompareOp::NotEqual: return a != b;
        case CompareOp::Less: return a < b;
        case CompareOp::LessEqual: return a <= b;
        case CompareOp::Greater: return a > b;
        case CompareOp::GreaterEqual: return a >= b;
        }
    }
    if (op == CompareOp::Equal) {
        return left == right;
    }
    if (op == CompareOp::NotEqual) {
        return left != right;
    }
    const auto* a = std::get_if<std::string>(&left);
    const auto* b = std::get_if<std::string>(&right);
    if (a && b) {
        switch (op) {
        case CompareOp::Less: return *a < *b;
        case CompareOp::LessEqual: return *a <= *b;
        case CompareOp::Greater: return *a > *b;
        case CompareOp::GreaterEqual: return *a >= *b;
        default: break;
        }
    }
    error = std::string(typeName(left)) + "와(과) " + typeName(right) + "의 크기를 비교할 수 없습니다";
    return false;
}

std::optional<Value> applyVariableOp(const Value& current, VariableOp op, const Value& operand, std::string& error) {
    error.clear();
    if (op == VariableOp::Set) {
        return operand;
    }
    // 아직 없는 변수는 0으로 보고 더하거나 뺀다.
    Value base = std::holds_alternative<std::monostate>(current) ? Value{std::int64_t{0}} : current;
    if (isNumber(base) && isNumber(operand)) {
        const auto* ai = std::get_if<std::int64_t>(&base);
        const auto* bi = std::get_if<std::int64_t>(&operand);
        if (ai && bi) {
            return Value{op == VariableOp::Add ? *ai + *bi : *ai - *bi};
        }
        double a = asDouble(base);
        double b = asDouble(operand);
        return Value{op == VariableOp::Add ? a + b : a - b};
    }
    if (op == VariableOp::Add) {
        const auto* a = std::get_if<std::string>(&base);
        const auto* b = std::get_if<std::string>(&operand);
        if (a && b) {
            return Value{*a + *b};
        }
    }
    error = std::string(typeName(base)) + "에 " + typeName(operand) + "을(를) " +
            (op == VariableOp::Add ? "더할" : "뺄") + " 수 없습니다";
    return std::nullopt;
}

} // namespace naru
