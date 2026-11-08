#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace naru {
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::string body;
    int timeoutMs = 10000;
};
struct HttpResult {
    std::string status = "pending";
    int statusCode = 0;
    std::string text;
    std::string error;
};
// Workers own transport state only. Cancellation discards results; it does not undo server writes.
// A synchronous native call may finish after cancellation, bounded by its transport timeout.
class Network {
public:
    Network();
    ~Network();
    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;
    std::uint64_t request(HttpRequest request);
    HttpResult result(std::uint64_t id, bool consume = false);
    void cancel(std::uint64_t id);
    void cancelAll();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace naru
