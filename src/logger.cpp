#include "common/logger.hpp"
#include "common/types.hpp"
#include <cstdio>
#include <chrono>

namespace trading {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() = default;

Logger::~Logger() {
    stop();
}

void Logger::start() {
    if (running_.exchange(true)) return; // Already running
    drain_thread_ = std::thread(&Logger::drain_loop, this);
}

void Logger::stop() {
    if (!running_.exchange(false)) return; // Already stopped
    if (drain_thread_.joinable()) {
        drain_thread_.join();
    }
    // Drain remaining entries
    LogEntry entry;
    while (queue_.try_pop(entry)) {
        fprintf(output_, "[%lu] %s\n", entry.timestamp_ns, entry.message);
    }
    fflush(output_);
}

void Logger::log(LogLevel level, const char* msg) {
    if (level < min_level_) return;

    LogEntry entry;
    entry.level = level;
    entry.timestamp_ns = now_ns();
    strncpy(entry.message, msg, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';

    queue_.try_push(entry); // Drop if queue full (acceptable for logging)
}

void Logger::drain_loop() {
    LogEntry entry;
    static const char* level_names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};

    while (running_.load(std::memory_order_relaxed)) {
        if (queue_.try_pop(entry)) {
            int idx = static_cast<int>(entry.level);
            if (idx < 0 || idx > 3) idx = 1;
            fprintf(output_, "[%s] [%lu] %s\n", level_names[idx], entry.timestamp_ns, entry.message);
        } else {
            // Yield to avoid busy-spinning on the log drain thread
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

} // namespace trading
