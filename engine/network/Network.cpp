#include "naru/network/Network.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#endif

namespace naru {
namespace {
using Clock = std::chrono::steady_clock;
constexpr std::size_t kMaxBody = 1024 * 1024;
std::atomic<int> activeWorkers{0};
std::atomic<std::uint64_t> nextId{1};
struct Operation {
    std::mutex mutex;
    HttpResult result;
    Clock::time_point deadline;
};
bool stopped(const std::shared_ptr<Operation>& op) {
    std::lock_guard lock(op->mutex);
    if (op->result.status != "pending") return true;
    if (Clock::now() >= op->deadline) {
        op->result.status = "failed";
        op->result.error = "timeout";
        return true;
    }
    return false;
}
#ifdef _WIN32
struct Handle {
    HINTERNET value = nullptr;
    ~Handle() { if (value) WinHttpCloseHandle(value); }
};
std::wstring wide(const std::string& value) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size && !value.empty()) throw std::invalid_argument("invalid_utf8");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}
HttpResult perform(const HttpRequest& input, const std::shared_ptr<Operation>& op) {
    HttpResult result;
    result.status = "failed";
    auto url = wide(input.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) throw std::invalid_argument("invalid_url");
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    if (parts.dwUserNameLength || parts.dwPasswordLength || (parts.dwExtraInfoLength && parts.lpszExtraInfo[0] == L'#')) throw std::invalid_argument("invalid_url");
    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    if (!secure && (parts.nScheme != INTERNET_SCHEME_HTTP || (host != L"127.0.0.1" && host != L"localhost" && host != L"[::1]" && host != L"::1"))) throw std::invalid_argument("https_required");
    Handle session{WinHttpOpen(L"NaruStudio/0.1", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) throw std::runtime_error("transport_error");
    if (!WinHttpSetTimeouts(session.value, input.timeoutMs, input.timeoutMs, input.timeoutMs, input.timeoutMs)) throw std::runtime_error("transport_error");
    Handle connection{WinHttpConnect(session.value, host.c_str(), parts.nPort, 0)};
    if (!connection.value) throw std::runtime_error("transport_error");
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (path.empty()) path = L"/";
    if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.find(L'#') != std::wstring::npos) throw std::invalid_argument("invalid_url");
    const auto method = wide(input.method);
    Handle request{WinHttpOpenRequest(connection.value, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0)};
    if (!request.value) throw std::runtime_error("transport_error");
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) throw std::runtime_error("transport_error");
    std::wstring headers;
    for (const auto& [name, value] : input.headers) headers += wide(name + ": " + value + "\r\n");
    if (stopped(op)) return result;
    if (!WinHttpSendRequest(request.value, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), static_cast<DWORD>(headers.size()), input.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(input.body.data()), static_cast<DWORD>(input.body.size()), static_cast<DWORD>(input.body.size()), 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) {
        result.error = GetLastError() == ERROR_WINHTTP_TIMEOUT ? "timeout" : "transport_error";
        return result;
    }
    DWORD status = 0, size = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) throw std::runtime_error("transport_error");
    result.statusCode = static_cast<int>(status);
    char buffer[16384];
    while (!stopped(op)) {
        DWORD count = 0;
        if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &count)) { result.error = GetLastError() == ERROR_WINHTTP_TIMEOUT ? "timeout" : "transport_error"; return result; }
        if (!count) break;
        if (result.text.size() + count > kMaxBody) { result.text.clear(); result.error = "response_too_large"; return result; }
        result.text.append(buffer, count);
    }
    if (stopped(op)) return result;
    if (status < 200 || status >= 300) result.error = "http_error";
    // The JSON parser treats a raw NUL as end-of-input. Reject it before parsing
    // so a valid prefix cannot hide trailing bytes. Escaped \u0000 remains valid.
    else if (!result.text.empty() && (result.text.find('\0') != std::string::npos || !nlohmann::json::accept(result.text))) result.error = "invalid_json";
    else result.status = "completed";
    return result;
}
#endif
void validate(const HttpRequest& request) {
    if (request.timeoutMs < 100 || request.timeoutMs > 60000) throw std::invalid_argument("timeout must be 0.1..60 seconds");
    if (request.url.empty() || request.url.size() > 8192 || request.url.find_first_of("\r\n\t ") != std::string::npos || request.url.find('\0') != std::string::npos) throw std::invalid_argument("invalid_url");
    if (request.method != "GET" && request.method != "POST" && request.method != "PUT" && request.method != "PATCH" && request.method != "DELETE") throw std::invalid_argument("invalid_method");
    if (request.body.size() > kMaxBody) throw std::invalid_argument("request_too_large");
    std::size_t size = 0;
    for (const auto& [name, value] : request.headers) {
        if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~") != std::string::npos || value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos) throw std::invalid_argument("invalid_header");
        size += name.size() + value.size();
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower == "host" || lower == "content-length" || lower == "transfer-encoding" || lower == "connection" || lower == "proxy-authorization") throw std::invalid_argument("reserved_header");
    }
    if (size > 16384) throw std::invalid_argument("headers_too_large");
}
} // namespace
struct Network::Impl { std::map<std::uint64_t, std::shared_ptr<Operation>> operations; };
Network::Network() : impl_(std::make_unique<Impl>()) {}
Network::~Network() { cancelAll(); }
std::uint64_t Network::request(HttpRequest input) {
    validate(input);
    if (impl_->operations.size() >= 32) throw std::runtime_error("request_limit: consume completed results");
    if (activeWorkers.fetch_add(1) >= 8) { --activeWorkers; throw std::runtime_error("worker_limit"); }
    const auto id = nextId.fetch_add(1);
    auto op = std::make_shared<Operation>();
    op->deadline = Clock::now() + std::chrono::milliseconds(input.timeoutMs);
    impl_->operations.emplace(id, op);
    try {
        std::thread([op, input = std::move(input)] {
            HttpResult result;
            result.status = "failed";
            try {
#ifdef _WIN32
                result = perform(input, op);
#else
                result.error = "unsupported_platform";
#endif
            } catch (const std::exception& error) { result.error = error.what(); }
            {
                std::lock_guard lock(op->mutex);
                if (op->result.status == "pending") {
                    if (Clock::now() >= op->deadline) { result.status = "failed"; result.error = "timeout"; }
                    op->result = std::move(result);
                }
            }
            --activeWorkers;
        }).detach();
    } catch (...) { impl_->operations.erase(id); --activeWorkers; throw; }
    return id;
}
HttpResult Network::result(std::uint64_t id, bool consume) {
    const auto found = impl_->operations.find(id);
    if (found == impl_->operations.end()) throw std::invalid_argument("unknown_request");
    auto op = found->second;
    stopped(op);
    std::lock_guard lock(op->mutex);
    auto result = op->result;
    if (consume && result.status != "pending") impl_->operations.erase(found);
    return result;
}
void Network::cancel(std::uint64_t id) {
    const auto found = impl_->operations.find(id);
    if (found == impl_->operations.end()) throw std::invalid_argument("unknown_request");
    std::lock_guard lock(found->second->mutex);
    if (found->second->result.status == "pending") { found->second->result.status = "cancelled"; found->second->result.error = "cancelled"; }
}
void Network::cancelAll() { for (const auto& [id, op] : impl_->operations) { (void)op; cancel(id); } }
} // namespace naru
