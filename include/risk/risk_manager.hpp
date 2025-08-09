#pragma once

#include "common/types.hpp"
#include "common/config.hpp"
#include "risk/position_tracker.hpp"
#include <atomic>
#include <array>

namespace trading {

enum class RiskCheckResult : uint8_t {
    Approved = 0,
    KillSwitchActive = 1,
    PositionLimitBreached = 2,
    CapitalLimitBreached = 3,
    OrderSizeTooLarge = 4,
    OrderRateExceeded = 5,
    FatFingerPrice = 6
};

/// Pre-trade risk manager. All checks must complete in <100ns.
/// Uses flat arrays, integer arithmetic, and [[unlikely]] on failure paths.
/// No virtual dispatch, no heap allocation, no division on hot path.
class RiskManager {
public:
    explicit RiskManager(const RiskLimits& limits);

    /// Check an order against all risk limits. O(1), <100ns target.
    __attribute__((hot))
    RiskCheckResult check_order(const OrderRequest& request, Price current_market_price) noexcept;

    /// Kill switch
    void activate_kill_switch() noexcept { kill_switch_.store(true, std::memory_order_release); }
    void deactivate_kill_switch() noexcept { kill_switch_.store(false, std::memory_order_release); }
    bool kill_switch_active() const noexcept { return kill_switch_.load(std::memory_order_acquire); }

    /// Drawdown monitoring
    void on_pnl_update(double total_pnl) noexcept;
    void set_peak_pnl(double peak) noexcept { peak_pnl_ = peak; }

    /// Position tracker access
    PositionTracker& position_tracker() noexcept { return positions_; }
    const PositionTracker& position_tracker() const noexcept { return positions_; }

    /// Update limits
    void set_limits(const RiskLimits& limits) noexcept { limits_ = limits; update_precomputed(); }
    const RiskLimits& limits() const noexcept { return limits_; }

    /// Reset rate counter (called periodically)
    void reset_rate_counter() noexcept { order_count_in_window_ = 0; rate_window_start_ = now_ns(); }

    uint64_t checks_performed() const noexcept { return checks_performed_; }
    uint64_t checks_rejected() const noexcept { return checks_rejected_; }

private:
    void update_precomputed() noexcept;

    RiskLimits limits_;
    PositionTracker positions_;

    alignas(64) std::atomic<bool> kill_switch_{false};

    // Precomputed reciprocals for avoiding division on hot path
    // price_deviation_reciprocal_ = 1.0 / (max_price_deviation_pct / 100.0)
    // We check: abs(order_price - market_price) * reciprocal > market_price
    // Which is equivalent to: abs(order_price - market_price) / market_price > pct/100
    double price_deviation_threshold_; // max_price_deviation_pct / 100.0

    // Rate limiting
    uint32_t order_count_in_window_ = 0;
    Timestamp rate_window_start_ = 0;

    // Drawdown
    double peak_pnl_ = 0.0;
    double max_drawdown_threshold_ = 0.0;

    // Stats
    uint64_t checks_performed_ = 0;
    uint64_t checks_rejected_ = 0;
};

} // namespace trading
