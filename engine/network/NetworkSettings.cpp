#include "naru/network/NetworkSettings.h"
#include <nlohmann/json.hpp>
#include <set>
#include <limits>
#include <stdexcept>

namespace naru {
namespace {
void validBase(const std::string& url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos || (url.substr(0, scheme) != "https" && url.substr(0, scheme) != "http"))
        throw std::invalid_argument("서버 주소는 http(s)://로 시작해야 합니다.");
    const auto end = url.find('/', scheme + 3);
    const auto authority = url.substr(scheme + 3, end == std::string::npos ? end : end - scheme - 3);
    if (authority.empty() || authority.find('@') != std::string::npos || url.find_first_of("?#") != std::string::npos)
        throw std::invalid_argument("서버 주소에 인증 정보·쿼리·fragment를 넣을 수 없습니다.");
    if (url.starts_with("http://") && authority != "localhost" && authority != "127.0.0.1" && authority != "[::1]" &&
        !authority.starts_with("localhost:") && !authority.starts_with("127.0.0.1:") && !authority.starts_with("[::1]:"))
        throw std::invalid_argument("외부 서버에는 HTTPS를 사용하세요.");
}
void validBody(const std::string& body) {
    if (!body.empty() && (body.find('\0') != std::string::npos || !nlohmann::json::accept(body)))
        throw std::invalid_argument("본문은 올바른 JSON이어야 합니다.");
}
}
void validateConnections(const std::vector<NetworkConnection>& connections) {
    std::set<std::string> ids;
    for (const auto& c : connections) {
        if (c.id.empty() || c.id.size() > 64 || c.id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos || !ids.insert(c.id).second)
            throw std::invalid_argument("접속 ID는 중복 없는 영문·숫자·_·- 1~64자여야 합니다.");
        validBase(c.baseUrl);
        Network::validateRequest({c.baseUrl, "GET", c.headers, "", c.timeoutMs});
    }
}
void validateNetworkEvent(const NetworkEvent& e) {
    if (e.connection.empty()) throw std::invalid_argument("접속 설정을 선택하세요.");
    if (e.path.empty() || e.path.front() != '/' || e.path.starts_with("//") || e.path.find('#') != std::string::npos)
        throw std::invalid_argument("경로는 /로 시작하고 //·fragment를 포함하지 않아야 합니다.");
    validBody(e.body);
    Network::validateRequest({"https://example.com" + e.path, e.method, e.headers, e.body});
    std::set<std::string> outputs;
    auto add = [&](const std::string& name) {
        if (name.empty() || name.size() > 64 || !outputs.insert(name).second)
            throw std::invalid_argument("결과 변수는 비어 있지 않은 64바이트 이하의 서로 다른 이름이어야 합니다.");
    };
    add(e.okVariable); add(e.codeVariable); add(e.errorVariable);
    for (const auto& [pointer, variable] : e.mappings) {
        (void)nlohmann::json::json_pointer(pointer);
        add(variable);
    }
    if (outputs.contains(e.bodyVariable)) throw std::invalid_argument("보낼 본문 변수와 결과 변수를 다르게 지정하세요.");
}
HttpRequest connectionRequest(const std::vector<NetworkConnection>& connections, const std::string& id,
                              const std::string& path, const std::string& method,
                              const std::map<std::string, std::string>& headers, const std::string& body) {
    validateConnections(connections);
    NetworkEvent e; e.connection = id; e.path = path; e.method = method; e.headers = headers; e.body = body;
    validateNetworkEvent(e);
    for (const auto& c : connections) if (c.id == id) {
        auto base = c.baseUrl;
        while (base.ends_with('/')) base.pop_back();
        HttpRequest r{base + path, method, c.headers, body, c.timeoutMs};
        // Header names are case insensitive; request-specific values override common values.
        for (const auto& [name, value] : headers) {
            std::erase_if(r.headers, [&](const auto& item) {
                auto lower = [](std::string s) { for (auto& ch : s) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A'; return s; };
                return lower(item.first) == lower(name);
            });
            r.headers[name] = value;
        }
        bool contentType = false;
        for (const auto& [name, value] : r.headers) {
            auto lower = name; for (auto& ch : lower) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            contentType |= lower == "content-type";
        }
        if (!body.empty() && !contentType) r.headers["Content-Type"] = "application/json; charset=utf-8";
        Network::validateRequest(r);
        return r;
    }
    throw std::invalid_argument("접속 설정을 찾을 수 없습니다: " + id);
}
void resetNetworkOutputs(const NetworkEvent& e, Variables& variables) {
    variables.set(e.okVariable, false); variables.set(e.codeVariable, std::int64_t{0}); variables.set(e.errorVariable, std::string{});
    for (const auto& [pointer, name] : e.mappings) variables.set(name, std::monostate{});
}
void applyNetworkResult(const NetworkEvent& e, HttpResult result, Variables& variables) {
    std::map<std::string, Value> mapped;
    if (result.status == "completed" && !e.mappings.empty()) {
        try {
            const auto data = result.text.empty() ? nlohmann::json(nullptr) : nlohmann::json::parse(result.text);
            for (const auto& [pointer, name] : e.mappings) {
                const auto& value = data.at(nlohmann::json::json_pointer(pointer));
                if (value.is_null()) mapped[name] = std::monostate{};
                else if (value.is_boolean()) mapped[name] = value.get<bool>();
                else if (value.is_number_unsigned()) {
                    if (value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) throw std::runtime_error("range");
                    mapped[name] = value.get<std::int64_t>();
                } else if (value.is_number_integer()) mapped[name] = value.get<std::int64_t>();
                else if (value.is_number_float()) mapped[name] = value.get<double>();
                else if (value.is_string()) mapped[name] = value.get<std::string>();
                else throw std::runtime_error("scalar required");
            }
        } catch (const std::exception&) { result.status = "failed"; result.error = "mapping_error"; mapped.clear(); }
    }
    resetNetworkOutputs(e, variables);
    variables.set(e.okVariable, result.status == "completed");
    variables.set(e.codeVariable, static_cast<std::int64_t>(result.statusCode));
    variables.set(e.errorVariable, result.error);
    for (const auto& [name, value] : mapped) variables.set(name, value);
}
} // namespace naru
