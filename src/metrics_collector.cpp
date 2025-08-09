#include "monitoring/metrics_collector.hpp"
#include <fstream>

namespace trading {

void MetricsCollector::print_summary(double elapsed_seconds) const {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           Ultra-Low Latency Trading System Report           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    printf("--- Throughput (%.2fs elapsed) ---\n", elapsed_seconds);
    if (elapsed_seconds > 0) {
        printf("  Market data:    %lu msgs  (%.0f msgs/sec)\n",
               static_cast<unsigned long>(md_msg_count_),
               static_cast<double>(md_msg_count_) / elapsed_seconds);
        printf("  Book updates:   %lu       (%.0f updates/sec)\n",
               static_cast<unsigned long>(ob_update_count_),
               static_cast<double>(ob_update_count_) / elapsed_seconds);
        printf("  Orders sent:    %lu       (%.0f orders/sec)\n",
               static_cast<unsigned long>(order_count_),
               static_cast<double>(order_count_) / elapsed_seconds);
        printf("  Fills:          %lu       (%.0f fills/sec)\n",
               static_cast<unsigned long>(fill_count_),
               static_cast<double>(fill_count_) / elapsed_seconds);
    }
    printf("\n");

    printf("--- Latency Statistics (nanoseconds) ---\n");
    printf("%-20s %10s %10s %10s %10s %10s %10s\n",
           "Component", "p50", "p90", "p95", "p99", "p99.9", "max");
    printf("%-20s %10s %10s %10s %10s %10s %10s\n",
           "─────────", "───", "───", "───", "───", "─────", "───");

    print_latency_stats("Market Data", md_latency_);
    print_latency_stats("Order Book", ob_latency_);
    print_latency_stats("Strategy", strategy_latency_);
    print_latency_stats("Risk Check", risk_latency_);
    print_latency_stats("Execution", exec_latency_);
    print_latency_stats("Tick-to-Trade", tick_to_trade_);

    printf("\n");
    tick_to_trade_hist_.print_report("Tick-to-Trade Histogram");
}

void MetricsCollector::print_latency_stats(const char* name, const LatencyTracker& tracker) const {
    if (tracker.count() == 0) {
        printf("%-20s %10s %10s %10s %10s %10s %10s\n", name, "N/A", "N/A", "N/A", "N/A", "N/A", "N/A");
        return;
    }

    auto stats = tracker.compute_stats();
    printf("%-20s %10lu %10lu %10lu %10lu %10lu %10lu\n",
           name,
           static_cast<unsigned long>(stats.p50),
           static_cast<unsigned long>(stats.p90),
           static_cast<unsigned long>(stats.p95),
           static_cast<unsigned long>(stats.p99),
           static_cast<unsigned long>(stats.p999),
           static_cast<unsigned long>(stats.max));
}

void MetricsCollector::dump_csv(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "component,p50,p90,p95,p99,p999,max,count\n";

    auto write_row = [&](const char* name, const LatencyTracker& tracker) {
        if (tracker.count() == 0) return;
        auto stats = tracker.compute_stats();
        file << name << ","
             << stats.p50 << "," << stats.p90 << "," << stats.p95 << ","
             << stats.p99 << "," << stats.p999 << "," << stats.max << ","
             << stats.count << "\n";
    };

    write_row("market_data", md_latency_);
    write_row("order_book", ob_latency_);
    write_row("strategy", strategy_latency_);
    write_row("risk_check", risk_latency_);
    write_row("execution", exec_latency_);
    write_row("tick_to_trade", tick_to_trade_);
}

void MetricsCollector::reset() noexcept {
    md_latency_.clear();
    ob_latency_.clear();
    strategy_latency_.clear();
    risk_latency_.clear();
    exec_latency_.clear();
    tick_to_trade_.clear();
    tick_to_trade_hist_.reset();
    md_msg_count_ = 0;
    ob_update_count_ = 0;
    order_count_ = 0;
    fill_count_ = 0;
}

} // namespace trading
