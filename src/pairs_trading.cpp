#include "strategy/pairs_trading.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace trading {

PairsTradingStrategy::PairsTradingStrategy(const Params& params)
    : params_(params)
{
    next_order_id_ = params.base_order_id;
}

void PairsTradingStrategy::on_market_data(const MarketDataMessage& md) {
    if (md.instrument == params_.instrument_a) {
        price_a_ = (md.bid_price + md.ask_price) / 2;
        if (price_a_ == 0 && md.last_price > 0) price_a_ = md.last_price;
    } else if (md.instrument == params_.instrument_b) {
        price_b_ = (md.bid_price + md.ask_price) / 2;
        if (price_b_ == 0 && md.last_price > 0) price_b_ = md.last_price;
    } else {
        return;
    }

    if (price_a_ > 0 && price_b_ > 0) {
        update_spread();
    }
}

void PairsTradingStrategy::on_order_book_update(InstrumentId instrument,
                                                  Price best_bid, Quantity bid_qty,
                                                  Price best_ask, Quantity ask_qty) {
    Price mid = (best_bid + best_ask) / 2;
    if (instrument == params_.instrument_a) {
        price_a_ = mid;
    } else if (instrument == params_.instrument_b) {
        price_b_ = mid;
    } else {
        return;
    }

    if (price_a_ > 0 && price_b_ > 0) {
        update_spread();
    }
}

void PairsTradingStrategy::on_trade(const Trade& trade) {}

void PairsTradingStrategy::on_execution_report(const ExecutionReport& report) {
    if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
        int qty = static_cast<int>(report.filled_quantity);
        if (report.instrument == params_.instrument_a) {
            position_a_ += (report.side == Side::Buy) ? qty : -qty;
        } else if (report.instrument == params_.instrument_b) {
            position_b_ += (report.side == Side::Buy) ? qty : -qty;
        }
    }
}

void PairsTradingStrategy::update_spread() {
    double spread = static_cast<double>(price_a_) - params_.hedge_ratio * static_cast<double>(price_b_);
    spreads_.push_back(spread);

    if (spreads_.size() < 20) {
        z_score_ = 0.0;
        return;
    }

    // Compute rolling mean and stddev
    double sum = 0.0;
    double sum_sq = 0.0;
    size_t n = spreads_.size();

    for (size_t i = 0; i < n; ++i) {
        double v = spreads_[i];
        sum += v;
        sum_sq += v * v;
    }

    double mean = sum / static_cast<double>(n);
    double variance = (sum_sq / static_cast<double>(n)) - (mean * mean);
    double stddev = std::sqrt(std::max(0.0, variance));

    if (stddev < 1e-10) {
        z_score_ = 0.0;
        return;
    }

    z_score_ = (spread - mean) / stddev;
}

std::span<const OrderRequest> PairsTradingStrategy::generate_orders() {
    order_count_ = 0;

    if (spreads_.size() < 20) return {};

    Timestamp now = now_ns();

    switch (state_) {
        case State::Flat:
            if (z_score_ > params_.entry_z_threshold) {
                // Spread is rich: sell A, buy B
                state_ = State::ShortSpread;
                {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_a;
                    req.side = Side::Sell;
                    req.type = OrderType::Limit;
                    req.price = price_a_;
                    req.quantity = params_.order_size;
                    req.timestamp = now;
                    req.exchange = 0;
                }
                {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_b;
                    req.side = Side::Buy;
                    req.type = OrderType::Limit;
                    req.price = price_b_;
                    req.quantity = static_cast<Quantity>(params_.order_size * params_.hedge_ratio);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            } else if (z_score_ < -params_.entry_z_threshold) {
                // Spread is cheap: buy A, sell B
                state_ = State::LongSpread;
                {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_a;
                    req.side = Side::Buy;
                    req.type = OrderType::Limit;
                    req.price = price_a_;
                    req.quantity = params_.order_size;
                    req.timestamp = now;
                    req.exchange = 0;
                }
                {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_b;
                    req.side = Side::Sell;
                    req.type = OrderType::Limit;
                    req.price = price_b_;
                    req.quantity = static_cast<Quantity>(params_.order_size * params_.hedge_ratio);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            }
            break;

        case State::ShortSpread:
            if (z_score_ < params_.exit_z_threshold) {
                // Flatten
                state_ = State::Flat;
                if (position_a_ < 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_a;
                    req.side = Side::Buy;
                    req.type = OrderType::Limit;
                    req.price = price_a_;
                    req.quantity = static_cast<Quantity>(-position_a_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
                if (position_b_ > 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_b;
                    req.side = Side::Sell;
                    req.type = OrderType::Limit;
                    req.price = price_b_;
                    req.quantity = static_cast<Quantity>(position_b_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            }
            break;

        case State::LongSpread:
            if (z_score_ > -params_.exit_z_threshold) {
                // Flatten
                state_ = State::Flat;
                if (position_a_ > 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_a;
                    req.side = Side::Sell;
                    req.type = OrderType::Limit;
                    req.price = price_a_;
                    req.quantity = static_cast<Quantity>(position_a_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
                if (position_b_ < 0) {
                    OrderRequest& req = order_buffer_[order_count_++];
                    req.id = alloc_order_id();
                    req.instrument = params_.instrument_b;
                    req.side = Side::Buy;
                    req.type = OrderType::Limit;
                    req.price = price_b_;
                    req.quantity = static_cast<Quantity>(-position_b_);
                    req.timestamp = now;
                    req.exchange = 0;
                }
            }
            break;
    }

    return std::span<const OrderRequest>(order_buffer_.data(), order_count_);
}

void PairsTradingStrategy::on_timer(Timestamp now) {}

} // namespace trading
