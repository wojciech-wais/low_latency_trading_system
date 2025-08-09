#pragma once

#include "monitoring/latency_tracker.hpp"
#include "monitoring/histogram.hpp"
#include <cstdio>
#include <string>

namespace trading {

/// Aggregates latency trackers, throughput counters, and histograms.
class MetricsCollector {
public:
    MetricsCollector() = default;

    // Latency trackers for each stage
    LatencyTracker& market_data_latency() { return md_latency_; }
    LatencyTracker& order_book_latency() { return ob_latency_; }
    LatencyTracker& strategy_latency() { return strategy_latency_; }
    LatencyTracker& risk_check_latency() { return risk_latency_; }
    LatencyTracker& execution_latency() { return exec_latency_; }
    LatencyTracker& tick_to_trade_latency() { return tick_to_trade_; }

    // Histograms
    Histogram& tick_to_trade_histogram() { return tick_to_trade_hist_; }

    // Throughput counters
    void record_market_data_msg() noexcept { ++md_msg_count_; }
    void record_order_book_update() noexcept { ++ob_update_count_; }
    void record_order_sent() noexcept { ++order_count_; }
    void record_fill() noexcept { ++fill_count_; }

    uint64_t market_data_messages() const noexcept { return md_msg_count_; }
    uint64_t order_book_updates() const noexcept { return ob_update_count_; }
    uint64_t orders_sent() const noexcept { return order_count_; }
    uint64_t fills() const noexcept { return fill_count_; }

    /// Print comprehensive summary
    void print_summary(double elapsed_seconds) const;

    /// Dump CSV of latency data
    void dump_csv(const std::string& path) const;

    void reset() noexcept;

private:
    void print_latency_stats(const char* name, const LatencyTracker& tracker) const;

    LatencyTracker md_latency_;
    LatencyTracker ob_latency_;
    LatencyTracker strategy_latency_;
    LatencyTracker risk_latency_;
    LatencyTracker exec_latency_;
    LatencyTracker tick_to_trade_;

    Histogram tick_to_trade_hist_;

    uint64_t md_msg_count_ = 0;
    uint64_t ob_update_count_ = 0;
    uint64_t order_count_ = 0;
    uint64_t fill_count_ = 0;
};

} // namespace trading
