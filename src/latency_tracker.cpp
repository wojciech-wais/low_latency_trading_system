#include "monitoring/latency_tracker.hpp"
#include <algorithm>
#include <vector>
#include <numeric>
#include <limits>

namespace trading {

void LatencyTracker::record(uint64_t latency_ns) noexcept {
    samples_.push_back(latency_ns);
}

LatencyTracker::Stats LatencyTracker::compute_stats() const {
    Stats stats{};
    size_t n = samples_.size();
    if (n == 0) return stats;

    // Copy to sortable vector (off hot path — only for reporting)
    std::vector<uint64_t> sorted(n);
    for (size_t i = 0; i < n; ++i) {
        sorted[i] = samples_[i];
    }
    std::sort(sorted.begin(), sorted.end());

    stats.count = n;
    stats.min = sorted[0];
    stats.max = sorted[n - 1];
    stats.p50 = sorted[n * 50 / 100];
    stats.p90 = sorted[n * 90 / 100];
    stats.p95 = sorted[n * 95 / 100];
    stats.p99 = sorted[n * 99 / 100];
    stats.p999 = sorted[std::min(n - 1, n * 999 / 1000)];

    double sum = 0.0;
    for (uint64_t v : sorted) sum += static_cast<double>(v);
    stats.mean = sum / static_cast<double>(n);

    return stats;
}

} // namespace trading
