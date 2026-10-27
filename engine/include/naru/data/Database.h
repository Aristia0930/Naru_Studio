#pragma once

#include "naru/core/Value.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace naru {

// 사용자가 정의하는 데이터베이스.
// "데이터 종류"(DataType)가 속성 목록(스키마)을 갖고, 각 항목(DataRecord)이 속성 값을 가진다.
// 캐릭터·아이템·몬스터 같은 구조를 엔진이 고정하지 않는다 (템플릿은 미리 만든 정의일 뿐).

enum class FieldType { Integer, Number, Text, Boolean, Choice, Reference };

struct FieldDef {
    std::string key;   // 스크립트·저장용 키. 만든 뒤 바꾸지 않는다 (예: "hp")
    std::string name;  // 표시 이름 (예: "체력")
    FieldType type = FieldType::Integer;
    Value defaultValue;
    std::optional<double> min; // Integer, Number
    std::optional<double> max;
    std::vector<std::string> choices; // Choice
    std::string refType;              // Reference: 참조할 데이터 종류 키

    bool operator==(const FieldDef&) const = default;
};

struct DataRecord {
    std::string id;   // 종류 안에서 고유 (예: "slime")
    std::string name; // 표시 이름 (예: "슬라임")
    std::map<std::string, Value> values; // 속성 키 → 값. 없으면 기본값

    bool operator==(const DataRecord&) const = default;
};

struct DataType {
    std::string key;  // 데이터 종류 키, 파일 이름에도 쓰인다 (예: "monster")
    std::string name; // 표시 이름 (예: "몬스터")
    std::vector<FieldDef> fields;
    std::vector<DataRecord> records;

    const FieldDef* findField(const std::string& fieldKey) const;
    int indexOfField(const std::string& fieldKey) const;
    DataRecord* findRecord(const std::string& recordId);
    const DataRecord* findRecord(const std::string& recordId) const;
    int indexOfRecord(const std::string& recordId) const;

    // 항목의 속성 값. 저장된 값이 없으면 기본값.
    Value valueOf(const DataRecord& record, const FieldDef& field) const;
    // prefix_001 형식의 쓰이지 않은 항목 ID
    std::string newRecordId(const std::string& prefix) const;

    bool operator==(const DataType&) const = default;
};

struct DataIssue {
    std::string location; // "monster/slime.drop" 같은 위치
    std::string message;
};

struct Database {
    std::vector<DataType> types;

    DataType* findType(const std::string& typeKey);
    const DataType* findType(const std::string& typeKey) const;
    int indexOfType(const std::string& typeKey) const;
    std::string newTypeKey(const std::string& prefix) const;

    // 참조 대상이 없거나 값이 형식에 맞지 않는 곳을 찾는다. 열기·저장을 막지는 않는 경고다.
    std::vector<DataIssue> validate() const;
    // typeKey를 참조하는 속성들 ("종류키.속성키")
    std::vector<std::string> referencesTo(const std::string& typeKey) const;

    bool operator==(const Database&) const = default;
};

const char* toKey(FieldType type);
std::optional<FieldType> fieldTypeFromKey(const std::string& key);

// 데이터 종류 키·항목 ID: 영문·숫자·_·- (파일 이름으로 쓰임)
bool isValidDataId(const std::string& id);
// 속성 키: 영문/_로 시작하는 영문·숫자·_ (스크립트에서 쓰기 쉬움). "id", "name"은 예약어
bool isValidFieldKey(const std::string& key);

// 형식의 기본 빈 값 (0, "", False, 첫 선택지, 참조 없음)
Value emptyValueFor(const FieldDef& field);
// 값이 속성 형식·범위·선택지에 맞는지. 맞지 않으면 이유를 돌려준다.
std::optional<std::string> checkValue(const Value& value, const FieldDef& field);
// 속성 정의가 바뀔 때 기존 값을 새 형식으로 바꾼다. 바꿀 수 없으면 nullopt.
std::optional<Value> convertValue(const Value& value, const FieldDef& target);

// 미리 만든 데이터 종류 정의 (자유롭게 고칠 수 있다). key는 비어 있으므로 호출한 쪽에서 정한다.
enum class DataTemplate { Empty, Character, Item, Monster };
DataType makeTemplate(DataTemplate kind, const Database& existing);

} // namespace naru
