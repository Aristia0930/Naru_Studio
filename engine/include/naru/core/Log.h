#pragma once

#include <functional>
#include <string>
#include <vector>

namespace naru {

enum class LogLevel { Info, Warning, Error };

const char* toString(LogLevel level);

// 로그가 어디서 나왔는지 기록한다. 스크립트 오류는 파일·행을, 이벤트 명령은 명령 ID를 채운다.
struct LogSource {
    std::string file;
    int line = 0;
    std::string commandId;
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string message;
    LogSource source;
    // 여러 줄 부가 정보 (예: Python traceback).
    std::string detail;
};

// 엔진·스크립트·에디터가 공유하는 로그 수집기. 에디터 패널이나 파일 로그는 sink로 연결한다.
class Logger {
public:
    using Sink = std::function<void(const LogEntry&)>;

    void addSink(Sink sink);
    void log(LogEntry entry);
    void info(std::string message, LogSource source = {});
    void warning(std::string message, LogSource source = {});
    void error(std::string message, LogSource source = {});

    const std::vector<LogEntry>& entries() const { return entries_; }
    void clear() { entries_.clear(); }

private:
    std::vector<LogEntry> entries_;
    std::vector<Sink> sinks_;
};

} // namespace naru
