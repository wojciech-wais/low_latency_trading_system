#include "strategy/market_maker.hpp"
#include <cmath>
#include <algorithm>

namespace trading {

MarketMakerStrategy::MarketMakerStrategy(const Params& params)
    : params_(params)
{
    next_order_id_ = params.base_order_id;
}

void MarketMakerStrategy::on_market_data(const MarketDataMessage& md) {
    if (md.instrument != params_.instrument) return;

    if (md.bid_price > 0 && md.ask_price > 0) {
        best_bid_ = md.bid_price;
        best_ask_ = md.ask_price;
        has_bbo_ = true;

        double mid = static_cast<double>(md.bid_price + md.ask_price) / 2.0;
        mid_prices_.push_back(mid);

        compute_fair_value();
        compute_dynamic_spread();
    }
}

void MarketMakerStrategy::on_order_book_update(InstrumentId instrument,
                                                 Price best_bid, Quantity bid_qty,
                                                 Price best_ask, Quantity ask_qty) {
    if (instrument != params_.instrument) return;
    best_bid_ = best_bid;
    best_ask_ = best_ask;
    has_bbo_ = (best_bid > 0 && best_ask > 0);

    if (has_bbo_) {
        double mid = static_cast<double>(best_bid + best_ask) / 2.0;
        mid_prices_.push_back(mid);
        compute_fair_value();
        compute_dynamic_spread();
    }
}

void MarketMakerStrategy::on_trade(const Trade& trade) {
    // Could adjust spread based on trade flow
}

void MarketMakerStrategy::on_execution_report(const ExecutionReport& report) {
    if (report.instrument != params_.instrument) return;
    if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
        if (report.side == Side::Buy) {
            inventory_ += static_cast<int>(report.filled_quantity);
        } else {
            inventory_ -= static_cast<int>(report.filled_quantity);
        }
    }
}

std::span<const OrderRequest> MarketMakerStrategy::generate_orders() {
    order_count_ = 0;

    if (!has_bbo_ || fair_value_ <= 0) {
        return {};
    }

    int abs_inventory = std::abs(inventory_);

    // Aggressive flatten if at inventory limit
    if (abs_inventory >= params_.max_inventory) {
        OrderRequest& req = order_buffer_[order_count_++];
        req.id = alloc_order_id();
        req.instrument = params_.instrument;
        req.type = OrderType::Limit;
        req.quantity = static_cast<Quantity>(abs_inventory);
        req.timestamp = now_ns();
        req.exchange = 0;

        if (inventory_ > 0) {
            // Need to sell
            req.side = Side::Sell;
            req.price = best_bid_; // Aggressive — hit the bid
        } else {
            // Need to buy
            req.side = Side::Buy;
            req.price = best_ask_; // Aggressive — lift the ask
        }
        return std::span<const OrderRequest>(order_buffer_.data(), order_count_);
    }

    // Compute spread with inventory skew
    double spread_ticks = current_spread_bps_ * static_cast<double>(fair_value_) / 10000.0;
    double half_spread = spread_ticks / 2.0;

    // Inventory skew: shift quotes to discourage increasing position
    double skew = params_.skew_factor * static_cast<double>(inventory_) * (spread_ticks / static_cast<double>(params_.max_inventory));

    Price bid_price = static_cast<Price>(static_cast<double>(fair_value_) - half_spread - skew);
    Price ask_price = static_cast<Price>(static_cast<double>(fair_value_) + half_spread - skew);

    // Ensure valid prices
    if (bid_price <= 0) bid_price = 1;
    if (ask_price <= bid_price) ask_price = bid_price + 1;

    // Post bid
    {
        OrderRequest& req = order_buffer_[order_count_++];
        req.id = alloc_order_id();
        req.instrument = params_.instrument;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.price = bid_price;
        req.quantity = params_.order_size;
        req.timestamp = now_ns();
        req.exchange = 0;
    }

    // Post ask
    {
        OrderRequest& req = order_buffer_[order_count_++];
        req.id = alloc_order_id();
        req.instrument = params_.instrument;
        req.side = Side::Sell;
        req.type = OrderType::Limit;
        req.price = ask_price;
        req.quantity = params_.order_size;
        req.timestamp = now_ns();
        req.exchange = 0;
    }

    return std::span<const OrderRequest>(order_buffer_.data(), order_count_);
}

void MarketMakerStrategy::on_timer(Timestamp now) {
    // Could implement stale quote cancellation
}

void MarketMakerStrategy::compute_fair_value() {
    if (has_bbo_) {
        fair_value_ = (best_bid_ + best_ask_) / 2;
    }
}

void MarketMakerStrategy::compute_dynamic_spread() {
    current_spread_bps_ = params_.base_spread_bps;

    if (mid_prices_.size() >= 10) {
        // Compute rolling volatility (stddev of returns)
        double sum = 0.0;
        double sum_sq = 0.0;
        size_t n = mid_prices_.size() - 1;

        for (size_t i = 1; i < mid_prices_.size(); ++i) {
            double ret = (mid_prices_[i] - mid_prices_[i - 1]) / mid_prices_[i - 1];
            sum += ret;
            sum_sq += ret * ret;
        }

        double mean = sum / static_cast<double>(n);
        double variance = (sum_sq / static_cast<double>(n)) - (mean * mean);
        double vol = std::sqrt(std::max(0.0, variance));

        // Scale spread by volatility (higher vol → wider spread)
        double vol_multiplier = 1.0 + vol * 10000.0; // bps scaling
        vol_multiplier = std::clamp(vol_multiplier, 1.0, 5.0);

        current_spread_bps_ = params_.base_spread_bps * vol_multiplier;
    }
}

} // namespace trading
