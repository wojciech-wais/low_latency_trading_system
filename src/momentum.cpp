#include "strategy/momentum.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace trading {

MomentumStrategy::MomentumStrategy(const Params& params)
    : params_(params)
{
    next_order_id_ = params.base_order_id;
    fast_alpha_ = 2.0 / (static_cast<double>(params.fast_window) + 1.0);
    slow_alpha_ = 2.0 / (static_cast<double>(params.slow_window) + 1.0);
}

void MomentumStrategy::on_market_data(const MarketDataMessage& md) {
    if (md.instrument != params_.instrument) return;

    Price mid = (md.bid_price + md.ask_price) / 2;
    if (mid <= 0 && md.last_price > 0) mid = md.last_price;
    if (mid <= 0) return;

    current_price_ = mid;
    update_emas(static_cast<double>(mid));

    if (md.last_quantity > 0) {
        volumes_.push_back(md.last_quantity);
    }
}

void MomentumStrategy::on_order_book_update(InstrumentId instrument,
                                              Price best_bid, Quantity bid_qty,
                                              Price best_ask, Quantity ask_qty) {
    if (instrument != params_.instrument) return;
    Price mid = (best_bid + best_ask) / 2;
    if (mid <= 0) return;

    current_price_ = mid;
    update_emas(static_cast<double>(mid));
}

void MomentumStrategy::on_trade(const Trade& trade) {
    if (trade.instrument != params_.instrument) return;
    volumes_.push_back(trade.quantity);
}

void MomentumStrategy::on_execution_report(const ExecutionReport& report) {
    if (report.instrument != params_.instrument) return;
    if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
        int qty = static_cast<int>(report.filled_quantity);
        position_ += (report.side == Side::Buy) ? qty : -qty;
    }
}

void MomentumStrategy::update_emas(double price) {
    ++tick_count_;

    if (tick_count_ == 1) {
        fast_ema_ = price;
        slow_ema_ = price;
    } else {
        fast_ema_ = fast_alpha_ * price + (1.0 - fast_alpha_) * fast_ema_;
        slow_ema_ = slow_alpha_ * price + (1.0 - slow_alpha_) * slow_ema_;
    }

    if (slow_ema_ > 1e-10) {
        momentum_signal_ = (fast_ema_ - slow_ema_) / slow_ema_ * 10000.0; // in bps
    } else {
        momentum_signal_ = 0.0;
    }

    // Update average volume
    if (volumes_.size() > 0) {
        double sum = 0.0;
        for (size_t i = 0; i < volumes_.size(); ++i) {
            sum += static_cast<double>(volumes_[i]);
        }
        avg_volume_ = sum / static_cast<double>(volumes_.size());
    }
}

std::span<const OrderRequest> MomentumStrategy::generate_orders() {
    order_count_ = 0;

    // Need enough data for EMAs to be meaningful
    if (tick_count_ < static_cast<uint64_t>(params_.slow_window)) return {};
    if (current_price_ <= 0) return {};

    Timestamp now = now_ns();
    double threshold = params_.breakout_threshold_bps;

    switch (state_) {
        case State::Flat:
            if (momentum_signal_ > threshold) {
                // Bullish momentum: go long
                state_ = State::Long;
                OrderRequest& req = order_buffer_[order_count_++];
                req.id = alloc_order_id();
                req.instrument = params_.instrument;
                req.side = Side::Buy;
                req.type = OrderType::Limit;
                req.price = current_price_;
                req.quantity = params_.order_size;
                req.timestamp = now;
                req.exchange = 0;
            } else if (momentum_signal_ < -threshold) {
                // Bearish momentum: go short
                state_ = State::Short;
                OrderRequest& req = order_buffer_[order_count_++];
                req.id = alloc_order_id();
                req.instrument = params_.instrument;
                req.side = Side::Sell;
                req.type = OrderType::Limit;
                req.price = current_price_;
                req.quantity = params_.order_size;
                req.timestamp = now;
                req.exchange = 0;
            }
            break;

        case State::Long:
            if (momentum_signal_ < 0) {
                // EMA crossover → flatten
                state_ = State::Flat;
                if (position_ > 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument;
                    req.side = Side::Sell;
                    req.type = OrderType::Limit;
                    req.price = current_price_;
                    req.quantity = static_cast<Quantity>(position_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            }
            break;

        case State::Short:
            if (momentum_signal_ > 0) {
                // EMA crossover → flatten
                state_ = State::Flat;
                if (position_ < 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument;
                    req.side = Side::Buy;
                    req.type = OrderType::Limit;
                    req.price = current_price_;
                    req.quantity = static_cast<Quantity>(-position_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            }
            break;
    }

    return std::span<const OrderRequest>(order_buffer_.data(), order_count_);
}

void MomentumStrategy::on_timer(Timestamp now) {}

} // namespace trading
