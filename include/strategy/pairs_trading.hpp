#pragma once

#include "strategy/strategy_interface.hpp"
#include "containers/circular_buffer.hpp"

namespace trading {

/// Pairs trading (statistical arbitrage) strategy:
/// - Tracks spread between two instruments (A - hedge_ratio * B)
/// - Computes rolling z-score
/// - Entry: |z| > entry_threshold → sell rich / buy cheap
/// - Exit: |z| < exit_threshold → flatten both legs
class PairsTradingStrategy : public StrategyInterface {
public:
    struct Params {
        InstrumentId instrument_a = 0;
        InstrumentId instrument_b = 1;
        double hedge_ratio = 1.0;
        size_t lookback_window = 100;
        double entry_z_threshold = 2.0;
        double exit_z_threshold = 0.5;
        Quantity order_size = 10;
        OrderId base_order_id = 200000;
    };

    explicit PairsTradingStrategy(const Params& params);

    void on_market_data(const MarketDataMessage& md) override;
    void on_order_book_update(InstrumentId instrument,
                               Price best_bid, Quantity bid_qty,
                               Price best_ask, Quantity ask_qty) override;
    void on_trade(const Trade& trade) override;
    void on_execution_report(const ExecutionReport& report) override;
    std::span<const OrderRequest> generate_orders() override;
    void on_timer(Timestamp now) override;
    std::string_view name() const override { return "PairsTrading"; }

    double z_score() const noexcept { return z_score_; }
    int position_a() const noexcept { return position_a_; }
    int position_b() const noexcept { return position_b_; }

private:
    void update_spread();

    Params params_;
    Price price_a_ = 0;
    Price price_b_ = 0;
    double z_score_ = 0.0;
    int position_a_ = 0;
    int position_b_ = 0;

    enum class State { Flat, LongSpread, ShortSpread };
    State state_ = State::Flat;

    CircularBuffer<double, 512> spreads_;
};

} // namespace trading
