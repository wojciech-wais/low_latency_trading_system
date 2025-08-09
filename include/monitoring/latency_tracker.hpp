#pragma once

#include "common/types.hpp"
#include "containers/circular_buffer.hpp"
#include <cstdint>

namespace trading {

/// Latency tracker: records latency samples in a circular buffer
/// and computes percentile statistics (p50, p90, p95, p99, p99.9, max).
class LatencyTracker {
public:
    static constexpr size_t MAX_SAMPLES = 1048576; // 1M samples

    LatencyTracker() noexcept = default;

    /// Record a latency sample (nanoseconds)
    void record(uint64_t latency_ns) noexcept;

    /// Compute statistics over current samples
    struct Stats {
        uint64_t p50 = 0;
        uint64_t p90 = 0;
        uint64_t p95 = 0;
        uint64_t p99 = 0;
        uint64_t p999 = 0;
        uint64_t max = 0;
        uint64_t min = 0;
        double mean = 0.0;
        size_t count = 0;
    };

    Stats compute_stats() const;

    size_t count() const noexcept { return samples_.size(); }
    void clear() noexcept { samples_.clear(); }

private:
    CircularBuffer<uint64_t, MAX_SAMPLES> samples_;
};

} // namespace trading
