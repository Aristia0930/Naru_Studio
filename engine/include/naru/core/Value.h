#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace naru {

// 게임 변수에 저장할 수 있는 값. Python 스크립트와 주고받을 수 있는 기본 형식만 둔다.
using Value = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

std::string toDisplayString(const Value& value);

// 이름 → 값. 편집 문서의 초기값과 실행 세션의 현재값을 각각 별도 인스턴스로 가진다.
class Variables {
public:
    std::optional<Value> get(const std::string& name) const;
    void set(const std::string& name, Value value);
    bool contains(const std::string& name) const { return values_.contains(name); }
    const std::map<std::string, Value>& all() const { return values_; }

    bool operator==(const Variables&) const = default;

private:
    std::map<std::string, Value> values_;
};

} // namespace naru
