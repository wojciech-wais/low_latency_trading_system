#pragma once

#include "strategy/strategy_interface.hpp"
#include "containers/circular_buffer.hpp"

namespace trading {

/// Market making strategy:
/// - Posts symmetric bid/ask around mid with dynamic spread
/// - Adjusts spread based on rolling volatility
/// - Skews quotes based on inventory
/// - Flattens aggressively at inventory limits
class MarketMakerStrategy : public StrategyInterface {
public:
    struct Params {
        double base_spread_bps = 10.0;    // Base spread in basis points
        int max_inventory = 100;           // Maximum position (absolute)
        Quantity order_size = 10;          // Quote size
        double skew_factor = 0.5;         // How much to skew for inventory
        size_t volatility_window = 100;   // Rolling window for vol estimate
        InstrumentId instrument = 0;
        OrderId base_order_id = 100000;
    };

    explicit MarketMakerStrategy(const Params& params);

    void on_market_data(const MarketDataMessage& md) override;
    void on_order_book_update(InstrumentId instrument,
                               Price best_bid, Quantity bid_qty,
                               Price best_ask, Quantity ask_qty) override;
    void on_trade(const Trade& trade) override;
    void on_execution_report(const ExecutionReport& report) override;
    std::span<const OrderRequest> generate_orders() override;
    void on_timer(Timestamp now) override;
    std::string_view name() const override { return "MarketMaker"; }

    int inventory() const noexcept { return inventory_; }
    double current_spread_bps() const noexcept { return current_spread_bps_; }

private:
    void compute_fair_value();
    void compute_dynamic_spread();

    Params params_;
    int inventory_ = 0;
    Price best_bid_ = 0;
    Price best_ask_ = 0;
    Price fair_value_ = 0;
    double current_spread_bps_ = 0.0;
    bool has_bbo_ = false;

    CircularBuffer<double, 256> mid_prices_;
};

} // namespace trading
