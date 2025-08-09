#pragma once

#include "common/types.hpp"
#include <string>
#include <array>

namespace trading {

struct ExchangeConfig {
    ExchangeId id = 0;
    std::string name;
    uint64_t latency_ns = 1000;         // Simulated latency in nanoseconds
    double fill_probability = 0.95;
    bool enabled = true;
};

struct RiskLimits {
    int64_t max_position_per_instrument = 10000;
    int64_t max_total_position = 50000;
    double max_capital = 10'000'000.0;
    Quantity max_order_size = 1000;
    uint32_t max_orders_per_second = 10000;
    double max_price_deviation_pct = 5.0;   // Fat finger: 5% from market
    double max_drawdown_pct = 2.0;          // 2% max drawdown triggers kill switch
};

struct SystemConfig {
    // Core assignments (even-numbered to avoid SMT siblings)
    int market_data_core = 2;
    int order_book_core = 4;
    int strategy_core = 6;
    int execution_core = 8;
    int monitoring_core = 10;

    // Queue sizes (must be power of 2)
    size_t market_data_queue_size = 65536;
    size_t order_queue_size = 65536;
    size_t execution_report_queue_size = 65536;

    // Exchanges
    static constexpr size_t MAX_EXCHANGES_CONFIG = 4;
    std::array<ExchangeConfig, MAX_EXCHANGES_CONFIG> exchanges;
    size_t num_exchanges = 2;

    // Risk limits
    RiskLimits risk_limits;

    // Feed simulator
    double feed_rate_msgs_per_sec = 1'000'000.0;
    uint32_t num_instruments = 2;
    double initial_price = 15000;   // Fixed-point: $150.00
    double volatility = 0.001;      // Per-tick volatility

    // Strategy config
    double market_maker_spread_bps = 10.0;
    int market_maker_max_inventory = 100;
    int pairs_lookback_window = 100;
    double pairs_entry_z = 2.0;
    double pairs_exit_z = 0.5;
    int momentum_fast_window = 10;
    int momentum_slow_window = 30;
    double momentum_breakout_bps = 5.0;

    // Paths
    std::string config_path;
    std::string data_path = "data/sample_market_data.csv";

    // Runtime
    uint64_t simulation_duration_ms = 10000;  // 10 seconds default
    bool enable_logging = true;
};

SystemConfig load_config(const std::string& path);
SystemConfig default_config();

} // namespace trading
