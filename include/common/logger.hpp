#pragma once

#include "containers/lock_free_queue.hpp"
#include <cstdio>
#include <cstring>
#include <atomic>
#include <thread>

namespace trading {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3
};

struct LogEntry {
    char message[240];
    LogLevel level;
    uint64_t timestamp_ns;
};

class Logger {
public:
    static Logger& instance();

    void start();
    void stop();

    void log(LogLevel level, const char* msg);

    void set_level(LogLevel level) { min_level_ = level; }
    LogLevel level() const { return min_level_; }

private:
    Logger();
    ~Logger();

    void drain_loop();

    LockFreeRingBuffer<LogEntry, 8192> queue_;
    std::atomic<bool> running_{false};
    std::thread drain_thread_;
    LogLevel min_level_ = LogLevel::Info;
    FILE* output_ = stderr;
};

// Macros for convenience
#ifdef NDEBUG
#define LOG_DEBUG(msg) ((void)0)
#else
#define LOG_DEBUG(msg) do { \
    if (::trading::Logger::instance().level() <= ::trading::LogLevel::Debug) \
        ::trading::Logger::instance().log(::trading::LogLevel::Debug, msg); \
} while(0)
#endif

#define LOG_INFO(msg)  do { ::trading::Logger::instance().log(::trading::LogLevel::Info, msg); } while(0)
#define LOG_WARN(msg)  do { ::trading::Logger::instance().log(::trading::LogLevel::Warn, msg); } while(0)
#define LOG_ERROR(msg) do { ::trading::Logger::instance().log(::trading::LogLevel::Error, msg); } while(0)

} // namespace trading
