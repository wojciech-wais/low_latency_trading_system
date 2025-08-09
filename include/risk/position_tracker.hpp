#pragma once

#include "common/types.hpp"
#include <array>
#include <cstdint>

namespace trading {

/// Flat-array position tracker indexed by InstrumentId. O(1) all operations.
/// No virtual dispatch, all inline for maximum performance.
class PositionTracker {
public:
    PositionTracker() noexcept;

    /// Update position on fill
    void on_fill(InstrumentId instrument, Side side, Quantity quantity, Price price) noexcept;

    /// Update mark-to-market prices
    void update_mark_price(InstrumentId instrument, Price price) noexcept;

    /// Position queries
    int64_t position(InstrumentId instrument) const noexcept;
    int64_t total_absolute_position() const noexcept;

    /// P&L
    double realized_pnl() const noexcept { return realized_pnl_; }
    double unrealized_pnl() const noexcept;
    double total_pnl() const noexcept;

    /// Average price for an instrument
    double avg_price(InstrumentId instrument) const noexcept;

    /// Capital used (approximate)
    double capital_used() const noexcept;

    /// Reset all positions
    void reset() noexcept;

private:
    std::array<int64_t, MAX_INSTRUMENTS> positions_;
    std::array<double, MAX_INSTRUMENTS> avg_prices_;       // Average entry price
    std::array<Price, MAX_INSTRUMENTS> mark_prices_;       // Current market price
    std::array<double, MAX_INSTRUMENTS> instrument_pnl_;   // Realized P&L per instrument
    double realized_pnl_ = 0.0;
};

} // namespace trading
