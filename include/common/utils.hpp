#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace trading {

inline bool pin_thread_to_core(int core_id) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)core_id;
    return false;
#endif
}

inline bool set_thread_realtime_priority(int priority) noexcept {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority;
    return pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
#else
    (void)priority;
    return false;
#endif
}

constexpr bool is_power_of_two(size_t n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

[[noreturn]] inline void fatal(const char* msg) noexcept {
    fprintf(stderr, "FATAL: %s\n", msg);
    fflush(stderr);
    abort();
}

} // namespace trading
