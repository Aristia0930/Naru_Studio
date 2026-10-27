#include "naru/events/EventData.h"

#include <array>
#include <utility>

namespace naru {

bool EventCommand::operator==(const EventCommand& o) const {
    return type == o.type && speaker == o.speaker && text == o.text && seconds == o.seconds &&
           variable == o.variable && op == o.op && compare == o.compare && value == o.value &&
           script == o.script && function == o.function && dataType == o.dataType && record == o.record &&
           field == o.field && target == o.target && direction == o.direction && distance == o.distance &&
           speed == o.speed && waitForEnd == o.waitForEnd && show == o.show && thenCommands == o.thenCommands &&
           elseCommands == o.elseCommands;
}

bool ObjectEvent::operator==(const ObjectEvent& o) const {
    return trigger == o.trigger && commands == o.commands;
}

namespace {

template <typename E, std::size_t N>
std::optional<E> lookup(const std::array<std::pair<E, const char*>, N>& table, const std::string& key) {
    for (const auto& [value, name] : table) {
        if (key == name) {
            return value;
        }
    }
    return std::nullopt;
}

template <typename E, std::size_t N>
const char* nameOf(const std::array<std::pair<E, const char*>, N>& table, E value) {
    for (const auto& [v, name] : table) {
        if (v == value) {
            return name;
        }
    }
    return "";
}

constexpr std::array<std::pair<EventCommandType, const char*>, 10> kTypes{{
    {EventCommandType::MoveObject, "move_object"},
    {EventCommandType::SetVisible, "set_visible"},
    {EventCommandType::GetData, "get_data"},
    {EventCommandType::Say, "say"},
    {EventCommandType::Wait, "wait"},
    {EventCommandType::SetVariable, "set_variable"},
    {EventCommandType::IfVariable, "if_variable"},
    {EventCommandType::Log, "log"},
    {EventCommandType::CallScript, "call_script"},
    {EventCommandType::Comment, "comment"},
}};

constexpr std::array<std::pair<VariableOp, const char*>, 3> kOps{{
    {VariableOp::Set, "set"},
    {VariableOp::Add, "add"},
    {VariableOp::Subtract, "subtract"},
}};

constexpr std::array<std::pair<CompareOp, const char*>, 6> kCompares{{
    {CompareOp::Equal, "=="},
    {CompareOp::NotEqual, "!="},
    {CompareOp::Less, "<"},
    {CompareOp::LessEqual, "<="},
    {CompareOp::Greater, ">"},
    {CompareOp::GreaterEqual, ">="},
}};

constexpr std::array<std::pair<EventTrigger, const char*>, 5> kTriggers{{
    {EventTrigger::Action, "action"},
    {EventTrigger::Touch, "touch"},
    {EventTrigger::Autorun, "autorun"},
    {EventTrigger::EventTouch, "event_touch"},
    {EventTrigger::Parallel, "parallel"},
}};

} // namespace

const char* toKey(EventCommandType type) { return nameOf(kTypes, type); }
const char* toKey(VariableOp op) { return nameOf(kOps, op); }
const char* toKey(CompareOp op) { return nameOf(kCompares, op); }
const char* toKey(EventTrigger trigger) { return nameOf(kTriggers, trigger); }
std::optional<EventCommandType> commandTypeFromKey(const std::string& key) { return lookup(kTypes, key); }
std::optional<VariableOp> variableOpFromKey(const std::string& key) { return lookup(kOps, key); }
std::optional<CompareOp> compareOpFromKey(const std::string& key) { return lookup(kCompares, key); }
std::optional<EventTrigger> triggerFromKey(const std::string& key) { return lookup(kTriggers, key); }

std::string commandId(const std::string& listPrefix, std::size_t index) {
    return listPrefix.empty() ? std::to_string(index) : listPrefix + "/" + std::to_string(index);
}

std::string thenPrefix(const std::string& id) { return id + "/t"; }
std::string elsePrefix(const std::string& id) { return id + "/e"; }

} // namespace naru
