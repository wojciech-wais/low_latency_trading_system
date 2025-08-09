#include "risk/risk_manager.hpp"
#include <cmath>

namespace trading {

RiskManager::RiskManager(const RiskLimits& limits)
    : limits_(limits)
{
    update_precomputed();
    rate_window_start_ = now_ns();
}

void RiskManager::update_precomputed() noexcept {
    price_deviation_threshold_ = limits_.max_price_deviation_pct / 100.0;
    max_drawdown_threshold_ = limits_.max_drawdown_pct / 100.0;
}

__attribute__((hot))
RiskCheckResult RiskManager::check_order(const OrderRequest& request, Price current_market_price) noexcept {
    ++checks_performed_;

    // 1. Kill switch (checked first, most critical)
    if (kill_switch_.load(std::memory_order_acquire)) [[unlikely]] {
        ++checks_rejected_;
        return RiskCheckResult::KillSwitchActive;
    }

    // 2. Order size check (cheapest: single comparison)
    if (request.quantity > limits_.max_order_size) [[unlikely]] {
        ++checks_rejected_;
        return RiskCheckResult::OrderSizeTooLarge;
    }

    // 3. Position limit check (array lookup + comparison)
    {
        int64_t current_pos = positions_.position(request.instrument);
        int64_t new_pos = current_pos;
        int64_t signed_qty = static_cast<int64_t>(request.quantity);
        if (request.side == Side::Buy) {
            new_pos += signed_qty;
        } else {
            new_pos -= signed_qty;
        }

        if (std::abs(new_pos) > limits_.max_position_per_instrument) [[unlikely]] {
            ++checks_rejected_;
            return RiskCheckResult::PositionLimitBreached;
        }

        // Total position check
        int64_t total = positions_.total_absolute_position();
        int64_t delta = std::abs(new_pos) - std::abs(current_pos);
        if (total + delta > limits_.max_total_position) [[unlikely]] {
            ++checks_rejected_;
            return RiskCheckResult::PositionLimitBreached;
        }
    }

    // 4. Capital limit check
    {
        double capital = positions_.capital_used();
        double order_value = static_cast<double>(request.quantity) *
                             static_cast<double>(request.price) / PRICE_SCALE;
        if (capital + order_value > limits_.max_capital) [[unlikely]] {
            ++checks_rejected_;
            return RiskCheckResult::CapitalLimitBreached;
        }
    }

    // 5. Order rate check
    {
        Timestamp now = now_ns();
        constexpr Timestamp ONE_SECOND_NS = 1'000'000'000ULL;
        if (now - rate_window_start_ >= ONE_SECOND_NS) {
            rate_window_start_ = now;
            order_count_in_window_ = 0;
        }
        ++order_count_in_window_;
        if (order_count_in_window_ > limits_.max_orders_per_second) [[unlikely]] {
            ++checks_rejected_;
            return RiskCheckResult::OrderRateExceeded;
        }
    }

    // 6. Fat finger check (price deviation from market)
    // Uses multiplication instead of division:
    // |order_price - market_price| > market_price * threshold
    if (current_market_price > 0) {
        int64_t price_diff = std::abs(request.price - current_market_price);
        // threshold check via multiplication to avoid division
        double diff_d = static_cast<double>(price_diff);
        double market_d = static_cast<double>(current_market_price);
        if (diff_d > market_d * price_deviation_threshold_) [[unlikely]] {
            ++checks_rejected_;
            return RiskCheckResult::FatFingerPrice;
        }
    }

    return RiskCheckResult::Approved;
}

void RiskManager::on_pnl_update(double total_pnl) noexcept {
    if (total_pnl > peak_pnl_) {
        peak_pnl_ = total_pnl;
    }

    // Check drawdown
    if (peak_pnl_ > 0.0) {
        double drawdown = (peak_pnl_ - total_pnl) / peak_pnl_;
        if (drawdown > max_drawdown_threshold_) {
            activate_kill_switch();
        }
    }
}

} // namespace trading
