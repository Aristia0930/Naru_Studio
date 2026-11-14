#pragma once
#include "naru/network/Network.h"
#include "naru/core/Value.h"
#include <vector>

namespace naru {
struct NetworkConnection {
    std::string id;
    std::string baseUrl;
    std::map<std::string, std::string> headers;
    int timeoutMs = 10000;
    bool operator==(const NetworkConnection&) const = default;
};
struct NetworkEvent {
    std::string connection;
    std::string path = "/";
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::string body; // Empty or literal JSON. No implicit variable interpolation.
    std::string bodyVariable; // Optional variable containing JSON text, overrides body.
    std::string okVariable = "api_ok";
    std::string codeVariable = "api_code";
    std::string errorVariable = "api_error";
    std::map<std::string, std::string> mappings; // JSON Pointer -> game variable.
    bool operator==(const NetworkEvent&) const = default;
};
void validateConnections(const std::vector<NetworkConnection>& connections);
void validateNetworkEvent(const NetworkEvent& event);
HttpRequest connectionRequest(const std::vector<NetworkConnection>& connections, const std::string& id,
                              const std::string& path, const std::string& method,
                              const std::map<std::string, std::string>& headers, const std::string& body);
// Maps scalar JSON atomically. Missing/non-scalar fields make the request fail.
// Output fields are reset at start so failures never expose stale data.
void resetNetworkOutputs(const NetworkEvent& event, Variables& variables);
void applyNetworkResult(const NetworkEvent& event, HttpResult result, Variables& variables);
} // namespace naru
