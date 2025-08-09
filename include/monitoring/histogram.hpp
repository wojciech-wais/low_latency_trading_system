#pragma once

#include <array>
#include <cstdint>
#include <cstdio>

namespace trading {

/// Log-scale fixed-bucket histogram for latency distribution.
/// Buckets: 0-10ns, 10-100ns, 100ns-1us, 1-10us, 10-100us, 100us-1ms, >1ms
class Histogram {
public:
    static constexpr size_t NUM_BUCKETS = 7;

    Histogram() noexcept { counts_.fill(0); }

    void record(uint64_t value_ns) noexcept {
        size_t bucket;
        if (value_ns < 10)              bucket = 0;
        else if (value_ns < 100)        bucket = 1;
        else if (value_ns < 1000)       bucket = 2;
        else if (value_ns < 10000)      bucket = 3;
        else if (value_ns < 100000)     bucket = 4;
        else if (value_ns < 1000000)    bucket = 5;
        else                            bucket = 6;

        ++counts_[bucket];
        ++total_count_;
        if (value_ns > max_) max_ = value_ns;
        if (value_ns < min_ || total_count_ == 1) min_ = value_ns;
    }

    void print_report(const char* title = "Latency Histogram") const {
        static const char* labels[] = {
            "  0-10ns  ",
            " 10-100ns ",
            "100ns-1us ",
            "  1-10us  ",
            " 10-100us ",
            "100us-1ms ",
            "  >1ms    "
        };

        printf("\n=== %s ===\n", title);
        printf("Total samples: %lu, Min: %luns, Max: %luns\n",
               static_cast<unsigned long>(total_count_),
               static_cast<unsigned long>(min_),
               static_cast<unsigned long>(max_));

        for (size_t i = 0; i < NUM_BUCKETS; ++i) {
            double pct = (total_count_ > 0)
                ? 100.0 * static_cast<double>(counts_[i]) / static_cast<double>(total_count_)
                : 0.0;

            // Simple bar chart
            int bar_len = static_cast<int>(pct / 2.0);
            printf("%s | %8lu (%5.1f%%) ", labels[i],
                   static_cast<unsigned long>(counts_[i]), pct);
            for (int j = 0; j < bar_len; ++j) printf("#");
            printf("\n");
        }
        printf("\n");
    }

    uint64_t count(size_t bucket) const noexcept {
        return (bucket < NUM_BUCKETS) ? counts_[bucket] : 0;
    }

    uint64_t total() const noexcept { return total_count_; }
    uint64_t max_value() const noexcept { return max_; }
    uint64_t min_value() const noexcept { return min_; }

    void reset() noexcept {
        counts_.fill(0);
        total_count_ = 0;
        max_ = 0;
        min_ = 0;
    }

private:
    std::array<uint64_t, NUM_BUCKETS> counts_;
    uint64_t total_count_ = 0;
    uint64_t max_ = 0;
    uint64_t min_ = 0;
};

} // namespace trading
