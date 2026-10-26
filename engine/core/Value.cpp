#include "naru/core/Value.h"

#include <utility>

namespace naru {

std::string toDisplayString(const Value& value) {
    struct Visitor {
        std::string operator()(std::monostate) const { return "None"; }
        std::string operator()(bool v) const { return v ? "True" : "False"; }
        std::string operator()(std::int64_t v) const { return std::to_string(v); }
        std::string operator()(double v) const { return std::to_string(v); }
        std::string operator()(const std::string& v) const { return v; }
    };
    return std::visit(Visitor{}, value);
}

std::optional<Value> Variables::get(const std::string& name) const {
    auto it = values_.find(name);
    if (it == values_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void Variables::set(const std::string& name, Value value) {
    values_[name] = std::move(value);
}

} // namespace naru
