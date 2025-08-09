#pragma once

#include "common/types.hpp"
#include <span>
#include <array>
#include <string_view>

namespace trading {

/// Abstract strategy interface. All strategies implement this.
/// generate_orders() returns a span over a pre-allocated member array — no heap alloc.
class StrategyInterface {
public:
    static constexpr size_t MAX_ORDERS_PER_SIGNAL = 8;

    virtual ~StrategyInterface() = default;

    /// Called on new market data
    virtual void on_market_data(const MarketDataMessage& md) = 0;

    /// Called when order book updates (BBO change)
    virtual void on_order_book_update(InstrumentId instrument,
                                       Price best_bid, Quantity bid_qty,
                                       Price best_ask, Quantity ask_qty) = 0;

    /// Called on trade
    virtual void on_trade(const Trade& trade) = 0;

    /// Called on execution report (fill/cancel/reject)
    virtual void on_execution_report(const ExecutionReport& report) = 0;

    /// Generate order requests based on current state.
    /// Returns span over pre-allocated internal buffer.
    virtual std::span<const OrderRequest> generate_orders() = 0;

    /// Periodic timer callback
    virtual void on_timer(Timestamp now) = 0;

    /// Strategy name for logging
    virtual std::string_view name() const = 0;

protected:
    std::array<OrderRequest, MAX_ORDERS_PER_SIGNAL> order_buffer_{};
    size_t order_count_ = 0;
    OrderId next_order_id_ = 1;

    OrderId alloc_order_id() noexcept { return next_order_id_++; }
};

} // namespace trading
