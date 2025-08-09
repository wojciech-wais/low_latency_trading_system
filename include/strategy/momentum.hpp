#pragma once

#include "strategy/strategy_interface.hpp"
#include "containers/circular_buffer.hpp"

namespace trading {

/// Momentum strategy:
/// - Fast EMA vs Slow EMA crossover
/// - Entry: momentum (fast-slow)/slow > breakout threshold + volume confirmation
/// - Exit: EMA crossover in opposite direction
class MomentumStrategy : public StrategyInterface {
public:
    struct Params {
        InstrumentId instrument = 0;
        int fast_window = 10;
        int slow_window = 30;
        double breakout_threshold_bps = 5.0;
        Quantity order_size = 10;
        OrderId base_order_id = 300000;
    };

    explicit MomentumStrategy(const Params& params);

    void on_market_data(const MarketDataMessage& md) override;
    void on_order_book_update(InstrumentId instrument,
                               Price best_bid, Quantity bid_qty,
                               Price best_ask, Quantity ask_qty) override;
    void on_trade(const Trade& trade) override;
    void on_execution_report(const ExecutionReport& report) override;
    std::span<const OrderRequest> generate_orders() override;
    void on_timer(Timestamp now) override;
    std::string_view name() const override { return "Momentum"; }

    double fast_ema() const noexcept { return fast_ema_; }
    double slow_ema() const noexcept { return slow_ema_; }
    double momentum_signal() const noexcept { return momentum_signal_; }
    int position() const noexcept { return position_; }

private:
    void update_emas(double price);

    Params params_;
    double fast_ema_ = 0.0;
    double slow_ema_ = 0.0;
    double fast_alpha_ = 0.0;
    double slow_alpha_ = 0.0;
    double momentum_signal_ = 0.0;
    int position_ = 0;
    uint64_t tick_count_ = 0;
    Price current_price_ = 0;

    enum class State { Flat, Long, Short };
    State state_ = State::Flat;

    // Volume tracking
    CircularBuffer<Quantity, 256> volumes_;
    double avg_volume_ = 0.0;
};

} // namespace trading
