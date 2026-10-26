#include "naru/core/Log.h"

#include <utility>

namespace naru {

const char* toString(LogLevel level) {
    switch (level) {
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error: return "error";
    }
    return "unknown";
}

void Logger::addSink(Sink sink) {
    sinks_.push_back(std::move(sink));
}

void Logger::log(LogEntry entry) {
    entries_.push_back(std::move(entry));
    for (const auto& sink : sinks_) {
        sink(entries_.back());
    }
}

void Logger::info(std::string message, LogSource source) {
    log({LogLevel::Info, std::move(message), std::move(source)});
}

void Logger::warning(std::string message, LogSource source) {
    log({LogLevel::Warning, std::move(message), std::move(source)});
}

void Logger::error(std::string message, LogSource source) {
    log({LogLevel::Error, std::move(message), std::move(source)});
}

} // namespace naru
