#pragma once

#include "naru/events/Command.h"
#include "naru/events/EventData.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace naru {

// "스크립트 호출" 명령을 실제로 실행하는 함수. 엔진이 Python에 직접 의존하지 않도록 밖에서 넣어 준다.
// 빈 문자열을 반환하면 성공, 아니면 오류 내용이다.
using ScriptRunner =
    std::function<std::string(const std::string& script, const std::string& function, CommandContext& ctx)>;

// 이벤트 명령 데이터를 실행 가능한 Command 목록으로 바꾼다.
// 조건 분기는 점프 명령으로 펼치며, 각 Command의 id()는 편집 화면의 명령 위치 ID(commandId)와 같다.
std::vector<std::unique_ptr<Command>> compileEvent(const std::vector<EventCommand>& commands,
                                                   const ScriptRunner& runScript);

// 변수 비교·연산 (테스트와 편집 화면 표시에도 사용). 형식이 맞지 않으면 error를 채우고 false/빈 값을 돌려준다.
bool compareValues(const Value& left, CompareOp op, const Value& right, std::string& error);
std::optional<Value> applyVariableOp(const Value& current, VariableOp op, const Value& operand, std::string& error);

} // namespace naru
