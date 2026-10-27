#include "naru/data/Database.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

namespace naru {

namespace {

constexpr std::array<std::pair<FieldType, const char*>, 6> kFieldTypes{{
    {FieldType::Integer, "integer"},
    {FieldType::Number, "number"},
    {FieldType::Text, "text"},
    {FieldType::Boolean, "boolean"},
    {FieldType::Choice, "choice"},
    {FieldType::Reference, "reference"},
}};

bool isAsciiAlnum(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

std::string numbered(const std::string& prefix, int n) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "_%03d", n);
    return prefix + buffer;
}

double clampToRange(double v, const FieldDef& f) {
    if (f.min) {
        v = std::max(v, *f.min);
    }
    if (f.max) {
        v = std::min(v, *f.max);
    }
    return v;
}

std::optional<double> numberOf(const Value& v) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return static_cast<double>(*i);
    }
    if (const auto* d = std::get_if<double>(&v)) {
        return *d;
    }
    return std::nullopt;
}

std::string rangeText(const FieldDef& f) {
    auto fmt = [](double v) {
        if (v == std::floor(v)) {
            return std::to_string(static_cast<long long>(v));
        }
        return std::to_string(v);
    };
    if (f.min && f.max) {
        return fmt(*f.min) + "~" + fmt(*f.max);
    }
    if (f.min) {
        return fmt(*f.min) + " 이상";
    }
    return fmt(*f.max) + " 이하";
}

FieldDef field(const char* key, const char* name, FieldType type, Value def) {
    FieldDef f;
    f.key = key;
    f.name = name;
    f.type = type;
    f.defaultValue = std::move(def);
    return f;
}

FieldDef intField(const char* key, const char* name, std::int64_t def, std::optional<double> min,
                  std::optional<double> max) {
    FieldDef f = field(key, name, FieldType::Integer, def);
    f.min = min;
    f.max = max;
    return f;
}

FieldDef choiceField(const char* key, const char* name, std::vector<std::string> choices) {
    FieldDef f = field(key, name, FieldType::Choice, choices.front());
    f.choices = std::move(choices);
    return f;
}

} // namespace

const char* toKey(FieldType type) {
    for (const auto& [t, name] : kFieldTypes) {
        if (t == type) {
            return name;
        }
    }
    return "";
}

std::optional<FieldType> fieldTypeFromKey(const std::string& key) {
    for (const auto& [t, name] : kFieldTypes) {
        if (key == name) {
            return t;
        }
    }
    return std::nullopt;
}

bool isValidDataId(const std::string& id) {
    if (id.empty() || id.size() > 64) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char c) { return isAsciiAlnum(c) || c == '_' || c == '-'; });
}

bool isValidFieldKey(const std::string& key) {
    if (key.empty() || key.size() > 64 || key == "id" || key == "name") {
        return false;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        return false;
    }
    return std::all_of(key.begin(), key.end(), [](char c) { return isAsciiAlnum(c) || c == '_'; });
}

// ---------------------------------------------------------------- DataType

const FieldDef* DataType::findField(const std::string& fieldKey) const {
    int i = indexOfField(fieldKey);
    return i < 0 ? nullptr : &fields[static_cast<std::size_t>(i)];
}

int DataType::indexOfField(const std::string& fieldKey) const {
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].key == fieldKey) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

DataRecord* DataType::findRecord(const std::string& recordId) {
    int i = indexOfRecord(recordId);
    return i < 0 ? nullptr : &records[static_cast<std::size_t>(i)];
}

const DataRecord* DataType::findRecord(const std::string& recordId) const {
    int i = indexOfRecord(recordId);
    return i < 0 ? nullptr : &records[static_cast<std::size_t>(i)];
}

int DataType::indexOfRecord(const std::string& recordId) const {
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (records[i].id == recordId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Value DataType::valueOf(const DataRecord& record, const FieldDef& f) const {
    auto it = record.values.find(f.key);
    return it != record.values.end() ? it->second : f.defaultValue;
}

std::string DataType::newRecordId(const std::string& prefix) const {
    for (int n = 1;; ++n) {
        std::string candidate = numbered(prefix, n);
        if (indexOfRecord(candidate) < 0) {
            return candidate;
        }
    }
}

// ---------------------------------------------------------------- Database

DataType* Database::findType(const std::string& typeKey) {
    int i = indexOfType(typeKey);
    return i < 0 ? nullptr : &types[static_cast<std::size_t>(i)];
}

const DataType* Database::findType(const std::string& typeKey) const {
    int i = indexOfType(typeKey);
    return i < 0 ? nullptr : &types[static_cast<std::size_t>(i)];
}

int Database::indexOfType(const std::string& typeKey) const {
    for (std::size_t i = 0; i < types.size(); ++i) {
        if (types[i].key == typeKey) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string Database::newTypeKey(const std::string& prefix) const {
    if (indexOfType(prefix) < 0) {
        return prefix;
    }
    for (int n = 2;; ++n) {
        std::string candidate = prefix + "_" + std::to_string(n);
        if (indexOfType(candidate) < 0) {
            return candidate;
        }
    }
}

std::vector<std::string> Database::referencesTo(const std::string& typeKey) const {
    std::vector<std::string> result;
    for (const auto& t : types) {
        for (const auto& f : t.fields) {
            if (f.type == FieldType::Reference && f.refType == typeKey) {
                result.push_back(t.key + "." + f.key);
            }
        }
    }
    return result;
}

std::vector<DataIssue> Database::validate() const {
    std::vector<DataIssue> issues;
    for (const auto& t : types) {
        for (const auto& f : t.fields) {
            std::string where = t.key + "." + f.key;
            if (f.type == FieldType::Choice && f.choices.empty()) {
                issues.push_back({where, "선택지가 없습니다"});
            }
            if (f.type == FieldType::Reference && !findType(f.refType)) {
                issues.push_back({where, f.refType.empty() ? "참조할 데이터 종류를 정하지 않았습니다"
                                                           : "참조할 데이터 종류가 없습니다: " + f.refType});
            }
            if (auto problem = checkValue(f.defaultValue, f)) {
                issues.push_back({where + " (기본값)", *problem});
            }
        }
        for (const auto& r : t.records) {
            for (const auto& [key, value] : r.values) {
                std::string where = t.key + "/" + r.id + "." + key;
                const FieldDef* f = t.findField(key);
                if (!f) {
                    issues.push_back({where, "정의되지 않은 속성입니다"});
                    continue;
                }
                if (auto problem = checkValue(value, *f)) {
                    issues.push_back({where, *problem});
                    continue;
                }
                if (f->type == FieldType::Reference) {
                    if (const auto* id = std::get_if<std::string>(&value)) {
                        const DataType* target = findType(f->refType);
                        if (target && !target->findRecord(*id)) {
                            issues.push_back({where, "없는 " + target->name + " 항목을 가리킵니다: " + *id});
                        }
                    }
                }
            }
        }
    }
    return issues;
}

// ---------------------------------------------------------------- 값 규칙

Value emptyValueFor(const FieldDef& f) {
    switch (f.type) {
    case FieldType::Integer: return static_cast<std::int64_t>(std::llround(clampToRange(0.0, f)));
    case FieldType::Number: return clampToRange(0.0, f);
    case FieldType::Text: return std::string();
    case FieldType::Boolean: return false;
    case FieldType::Choice: return f.choices.empty() ? std::string() : f.choices.front();
    case FieldType::Reference: return std::monostate{};
    }
    return std::monostate{};
}

std::optional<std::string> checkValue(const Value& value, const FieldDef& f) {
    auto checkRange = [&f](double v) -> std::optional<std::string> {
        if ((f.min && v < *f.min) || (f.max && v > *f.max)) {
            return "범위(" + rangeText(f) + ")를 벗어났습니다: " + toDisplayString(Value{v});
        }
        return std::nullopt;
    };
    switch (f.type) {
    case FieldType::Integer:
        if (const auto* i = std::get_if<std::int64_t>(&value)) {
            return checkRange(static_cast<double>(*i));
        }
        return "정수여야 합니다";
    case FieldType::Number:
        if (auto n = numberOf(value)) {
            return checkRange(*n);
        }
        return "숫자여야 합니다";
    case FieldType::Text:
        if (std::holds_alternative<std::string>(value)) {
            return std::nullopt;
        }
        return "문자열이어야 합니다";
    case FieldType::Boolean:
        if (std::holds_alternative<bool>(value)) {
            return std::nullopt;
        }
        return "참/거짓이어야 합니다";
    case FieldType::Choice:
        if (const auto* s = std::get_if<std::string>(&value)) {
            if (std::find(f.choices.begin(), f.choices.end(), *s) != f.choices.end()) {
                return std::nullopt;
            }
            return "선택지에 없는 값입니다: " + *s;
        }
        return "선택지 중 하나여야 합니다";
    case FieldType::Reference:
        if (std::holds_alternative<std::monostate>(value) || std::holds_alternative<std::string>(value)) {
            return std::nullopt;
        }
        return "항목 ID(문자열)이거나 비어 있어야 합니다";
    }
    return "알 수 없는 형식";
}

std::optional<Value> convertValue(const Value& value, const FieldDef& target) {
    if (!checkValue(value, target)) {
        return value;
    }
    switch (target.type) {
    case FieldType::Integer:
    case FieldType::Number: {
        std::optional<double> n = numberOf(value);
        if (const auto* b = std::get_if<bool>(&value)) {
            n = *b ? 1.0 : 0.0;
        } else if (const auto* s = std::get_if<std::string>(&value)) {
            try {
                std::size_t used = 0;
                double parsed = std::stod(*s, &used);
                if (used == s->size()) {
                    n = parsed;
                }
            } catch (...) {
            }
        }
        if (!n) {
            return std::nullopt;
        }
        double clamped = clampToRange(*n, target);
        if (target.type == FieldType::Integer) {
            return Value{static_cast<std::int64_t>(std::llround(clamped))};
        }
        return Value{clamped};
    }
    case FieldType::Text:
        if (std::holds_alternative<std::monostate>(value)) {
            return Value{std::string()};
        }
        return Value{toDisplayString(value)};
    case FieldType::Boolean:
        if (auto n = numberOf(value)) {
            return Value{*n != 0.0};
        }
        return std::nullopt;
    case FieldType::Choice:
    case FieldType::Reference:
        return std::nullopt;
    }
    return std::nullopt;
}

// ---------------------------------------------------------------- 템플릿

DataType makeTemplate(DataTemplate kind, const Database& existing) {
    DataType t;
    switch (kind) {
    case DataTemplate::Empty:
        t.key = existing.newTypeKey("data");
        t.name = "새 데이터";
        break;
    case DataTemplate::Character:
        t.key = existing.newTypeKey("character");
        t.name = "캐릭터";
        t.fields = {
            choiceField("job", "직업", {"전사", "마법사", "궁수"}),
            intField("level", "레벨", 1, 1, 99),
            intField("hp", "체력", 100, 1, 99999),
            intField("mp", "마력", 20, 0, 9999),
            intField("attack", "공격력", 10, 0, 9999),
            intField("defense", "방어력", 10, 0, 9999),
            field("description", "설명", FieldType::Text, std::string()),
        };
        break;
    case DataTemplate::Item:
        t.key = existing.newTypeKey("item");
        t.name = "아이템";
        t.fields = {
            choiceField("category", "분류", {"소모품", "장비", "중요"}),
            intField("price", "가격", 0, 0, 999999),
            field("consumable", "사용하면 사라짐", FieldType::Boolean, true),
            intField("heal", "회복량", 0, 0, 99999),
            field("description", "설명", FieldType::Text, std::string()),
        };
        break;
    case DataTemplate::Monster: {
        t.key = existing.newTypeKey("monster");
        t.name = "몬스터";
        FieldDef drop = field("drop", "떨어뜨리는 아이템", FieldType::Reference, std::monostate{});
        // 아이템 종류가 있으면 그것을 참조한다. 없으면 사용자가 나중에 고른다.
        for (const auto& other : existing.types) {
            if (other.key == "item" || other.name == "아이템") {
                drop.refType = other.key;
                break;
            }
        }
        t.fields = {
            intField("hp", "체력", 30, 1, 999999),
            intField("attack", "공격력", 5, 0, 9999),
            choiceField("element", "속성", {"없음", "불", "물", "풀"}),
            intField("exp", "경험치", 10, 0, 999999),
            intField("gold", "골드", 5, 0, 999999),
            drop,
        };
        break;
    }
    }
    return t;
}

} // namespace naru
