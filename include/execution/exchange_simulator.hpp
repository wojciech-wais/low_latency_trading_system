#pragma once

#include "common/types.hpp"
#include "common/config.hpp"
#include "order_book/order_book.hpp"
#include <random>

namespace trading {

/// Simulates a single exchange with configurable latency and fill behavior.
/// Uses an internal OrderBook for realistic matching.
class ExchangeSimulator {
public:
    explicit ExchangeSimulator(const ExchangeConfig& config);

    /// Submit an order. Returns execution report after simulated latency.
    ExecutionReport submit_order(const OrderRequest& request);

    /// Cancel an order. Returns execution report.
    ExecutionReport cancel_order(OrderId order_id);

    /// Seed the book with resting orders for realistic simulation.
    void seed_book(Price mid_price, int levels, Quantity qty_per_level);

    /// Update the book with a market data message (for price tracking).
    void update_book(const MarketDataMessage& md);

    ExchangeId id() const noexcept { return config_.id; }
    const ExchangeConfig& config() const noexcept { return config_; }
    uint64_t orders_processed() const noexcept { return orders_processed_; }
    uint64_t fills() const noexcept { return fills_; }
    uint64_t rejects() const noexcept { return rejects_; }

private:
    ExchangeConfig config_;
    OrderBook book_;
    std::mt19937 rng_;
    OrderId next_exec_id_ = 1;
    uint64_t orders_processed_ = 0;
    uint64_t fills_ = 0;
    uint64_t rejects_ = 0;
};

} // namespace trading
