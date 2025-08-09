#include "common/config.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

namespace trading {

// Simple JSON-like parser (no external dependency for core config)
// Supports: {"key": value, "key": "string", "key": number}
namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

double parse_number(const std::string& s) {
    return std::stod(s);
}

int parse_int(const std::string& s) {
    return std::stoi(s);
}

} // anonymous namespace

SystemConfig default_config() {
    SystemConfig config;

    // Default exchange configs
    config.exchanges[0] = {0, "SIM_NYSE", 500, 0.95, true};
    config.exchanges[1] = {1, "SIM_NASDAQ", 300, 0.98, true};
    config.exchanges[2] = {2, "SIM_BATS", 200, 0.92, true};
    config.exchanges[3] = {3, "SIM_ARCA", 400, 0.90, true};
    config.num_exchanges = 2;

    return config;
}

SystemConfig load_config(const std::string& path) {
    SystemConfig config = default_config();

    std::ifstream file(path);
    if (!file.is_open()) {
        return config; // Return defaults if file not found
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Minimal key-value extraction from JSON
    auto extract_value = [&](const std::string& key) -> std::string {
        std::string search_key = "\"" + key + "\"";
        size_t pos = content.find(search_key);
        if (pos == std::string::npos) return "";
        pos = content.find(':', pos);
        if (pos == std::string::npos) return "";
        pos++;
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;
        if (pos >= content.size()) return "";

        if (content[pos] == '"') {
            size_t end = content.find('"', pos + 1);
            if (end == std::string::npos) return "";
            return content.substr(pos + 1, end - pos - 1);
        }

        size_t end = content.find_first_of(",}\n", pos);
        if (end == std::string::npos) end = content.size();
        return trim(content.substr(pos, end - pos));
    };

    auto try_int = [&](const std::string& key, int& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = parse_int(val);
    };

    auto try_size = [&](const std::string& key, size_t& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = static_cast<size_t>(parse_int(val));
    };

    auto try_uint32 = [&](const std::string& key, uint32_t& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = static_cast<uint32_t>(parse_int(val));
    };

    auto try_double = [&](const std::string& key, double& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = parse_number(val);
    };

    auto try_int64 = [&](const std::string& key, int64_t& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = static_cast<int64_t>(parse_number(val));
    };

    auto try_quantity = [&](const std::string& key, Quantity& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = static_cast<Quantity>(parse_number(val));
    };

    auto try_uint64 = [&](const std::string& key, uint64_t& target) {
        std::string val = extract_value(key);
        if (!val.empty()) target = static_cast<uint64_t>(parse_number(val));
    };

    // Core assignments
    try_int("market_data_core", config.market_data_core);
    try_int("order_book_core", config.order_book_core);
    try_int("strategy_core", config.strategy_core);
    try_int("execution_core", config.execution_core);
    try_int("monitoring_core", config.monitoring_core);

    // Queue sizes
    try_size("market_data_queue_size", config.market_data_queue_size);
    try_size("order_queue_size", config.order_queue_size);
    try_size("execution_report_queue_size", config.execution_report_queue_size);

    // Risk limits
    try_int64("max_position_per_instrument", config.risk_limits.max_position_per_instrument);
    try_int64("max_total_position", config.risk_limits.max_total_position);
    try_double("max_capital", config.risk_limits.max_capital);
    try_quantity("max_order_size", config.risk_limits.max_order_size);
    try_uint32("max_orders_per_second", config.risk_limits.max_orders_per_second);
    try_double("max_price_deviation_pct", config.risk_limits.max_price_deviation_pct);
    try_double("max_drawdown_pct", config.risk_limits.max_drawdown_pct);

    // Feed simulator
    try_double("feed_rate_msgs_per_sec", config.feed_rate_msgs_per_sec);
    try_uint32("num_instruments", config.num_instruments);
    try_double("initial_price", config.initial_price);
    try_double("volatility", config.volatility);

    // Strategy
    try_double("market_maker_spread_bps", config.market_maker_spread_bps);
    try_int("market_maker_max_inventory", config.market_maker_max_inventory);
    try_int("pairs_lookback_window", config.pairs_lookback_window);
    try_double("pairs_entry_z", config.pairs_entry_z);
    try_double("pairs_exit_z", config.pairs_exit_z);
    try_int("momentum_fast_window", config.momentum_fast_window);
    try_int("momentum_slow_window", config.momentum_slow_window);
    try_double("momentum_breakout_bps", config.momentum_breakout_bps);

    // Runtime
    try_uint64("simulation_duration_ms", config.simulation_duration_ms);

    config.config_path = path;
    return config;
}

} // namespace trading
