#pragma once

#include "naru/core/Value.h"

#include <optional>
#include <string>
#include <vector>

namespace naru {

// 편집·저장되는 이벤트 명령 데이터. 실행할 때 EventCompiler가 Command 목록으로 바꾼다.
enum class EventCommandType {
    Say, Wait, SetVariable, IfVariable, Log, CallScript, Comment, GetData, MoveObject, SetVisible
};
enum class VariableOp { Set, Add, Subtract };
enum class CompareOp { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

struct EventCommand {
    EventCommandType type = EventCommandType::Comment;

    std::string speaker;            // Say
    std::string text;               // Say 대사, Log 메시지, Comment 내용
    double seconds = 0.0;           // Wait
    std::string variable;           // SetVariable, IfVariable, GetData·CallScript(결과를 넣을 변수, CallScript는 비어 있으면 버림)
    std::string dataType;           // GetData: 데이터 종류 키
    std::string record;             // GetData: 항목 ID
    std::string field;              // GetData: 속성 키 ("id", "name"도 가능)
    VariableOp op = VariableOp::Set; // SetVariable
    CompareOp compare = CompareOp::Equal; // IfVariable
    Value value;                    // SetVariable, IfVariable
    std::string script;             // CallScript: scripts/ 안의 파일 이름
    std::string function = "run";   // CallScript
    // MoveObject, SetVisible: 대상 객체 ID 또는 kTargetSelf·kTargetPlayer
    std::string target = "@self";
    std::string direction = "right"; // MoveObject: "up" "down" "left" "right"
    double distance = 48.0;          // MoveObject: 픽셀
    double speed = 240.0;            // MoveObject: 픽셀/초, 0이면 순간 이동
    bool waitForEnd = true;          // MoveObject: 이동이 끝날 때까지 다음 명령으로 넘어가지 않음
    bool show = true;                // SetVisible: true 보이기, false 숨기기
    std::vector<EventCommand> thenCommands; // IfVariable
    std::vector<EventCommand> elseCommands; // IfVariable

    bool operator==(const EventCommand& other) const;
};

// 명령 대상 객체의 특별한 이름: 이 이벤트가 붙은 객체, 장면의 플레이어 객체.
inline constexpr const char* kTargetSelf = "@self";
inline constexpr const char* kTargetPlayer = "@player";

// 이벤트가 시작되는 조건. 게임 실행 화면이 생기면 런타임이 이 값을 사용한다.
// (순서는 저장 키와 무관하지만 편집 화면 목록 순서와 같다)
enum class EventTrigger { Action, Touch, Autorun, EventTouch, Parallel };

struct ObjectEvent {
    EventTrigger trigger = EventTrigger::Action;
    std::vector<EventCommand> commands;

    bool operator==(const ObjectEvent& other) const;
};

// 저장 형식의 문자열 이름 ("say", "if_variable", ">=" ...).
const char* toKey(EventCommandType type);
const char* toKey(VariableOp op);
const char* toKey(CompareOp op);
const char* toKey(EventTrigger trigger);
std::optional<EventCommandType> commandTypeFromKey(const std::string& key);
std::optional<VariableOp> variableOpFromKey(const std::string& key);
std::optional<CompareOp> compareOpFromKey(const std::string& key);
std::optional<EventTrigger> triggerFromKey(const std::string& key);

// 명령 위치 ID. 최상위 i번째는 "i", 조건 분기 안은 "<분기 ID>/t/<i>" 또는 "<분기 ID>/e/<i>".
// 실행 오류 위치를 편집 화면의 명령과 연결하는 데 쓴다.
std::string commandId(const std::string& listPrefix, std::size_t index);
std::string thenPrefix(const std::string& id);
std::string elsePrefix(const std::string& id);

} // namespace naru
