#include "risk/position_tracker.hpp"
#include <cmath>
#include <cstring>

namespace trading {

PositionTracker::PositionTracker() noexcept {
    reset();
}

void PositionTracker::reset() noexcept {
    positions_.fill(0);
    avg_prices_.fill(0.0);
    mark_prices_.fill(0);
    instrument_pnl_.fill(0.0);
    realized_pnl_ = 0.0;
}

void PositionTracker::on_fill(InstrumentId instrument, Side side, Quantity quantity, Price price) noexcept {
    if (instrument >= MAX_INSTRUMENTS) return;

    int64_t signed_qty = static_cast<int64_t>(quantity);
    double fill_price = static_cast<double>(price) / PRICE_SCALE;
    int64_t& pos = positions_[instrument];
    double& avg = avg_prices_[instrument];

    if (side == Side::Buy) {
        if (pos >= 0) {
            // Adding to long position — update average
            double total_cost = avg * static_cast<double>(pos) + fill_price * static_cast<double>(signed_qty);
            pos += signed_qty;
            if (pos > 0) {
                avg = total_cost / static_cast<double>(pos);
            }
        } else {
            // Covering short — realize P&L
            int64_t cover_qty = std::min(signed_qty, -pos);
            double pnl = static_cast<double>(cover_qty) * (avg - fill_price);
            realized_pnl_ += pnl;
            instrument_pnl_[instrument] += pnl;
            pos += signed_qty;
            if (pos > 0) {
                avg = fill_price; // New long from scratch
            } else if (pos == 0) {
                avg = 0.0;
            }
        }
    } else {
        // Sell
        if (pos <= 0) {
            // Adding to short position
            double total_cost = avg * static_cast<double>(-pos) + fill_price * static_cast<double>(signed_qty);
            pos -= signed_qty;
            if (pos < 0) {
                avg = total_cost / static_cast<double>(-pos);
            }
        } else {
            // Selling long — realize P&L
            int64_t sell_qty = std::min(signed_qty, pos);
            double pnl = static_cast<double>(sell_qty) * (fill_price - avg);
            realized_pnl_ += pnl;
            instrument_pnl_[instrument] += pnl;
            pos -= signed_qty;
            if (pos < 0) {
                avg = fill_price; // New short from scratch
            } else if (pos == 0) {
                avg = 0.0;
            }
        }
    }
}

void PositionTracker::update_mark_price(InstrumentId instrument, Price price) noexcept {
    if (instrument < MAX_INSTRUMENTS) {
        mark_prices_[instrument] = price;
    }
}

int64_t PositionTracker::position(InstrumentId instrument) const noexcept {
    if (instrument >= MAX_INSTRUMENTS) return 0;
    return positions_[instrument];
}

int64_t PositionTracker::total_absolute_position() const noexcept {
    int64_t total = 0;
    for (size_t i = 0; i < MAX_INSTRUMENTS; ++i) {
        total += std::abs(positions_[i]);
    }
    return total;
}

double PositionTracker::unrealized_pnl() const noexcept {
    double pnl = 0.0;
    for (size_t i = 0; i < MAX_INSTRUMENTS; ++i) {
        if (positions_[i] != 0 && mark_prices_[i] != 0) {
            double mark = static_cast<double>(mark_prices_[i]) / PRICE_SCALE;
            if (positions_[i] > 0) {
                pnl += static_cast<double>(positions_[i]) * (mark - avg_prices_[i]);
            } else {
                pnl += static_cast<double>(-positions_[i]) * (avg_prices_[i] - mark);
            }
        }
    }
    return pnl;
}

double PositionTracker::total_pnl() const noexcept {
    return realized_pnl_ + unrealized_pnl();
}

double PositionTracker::avg_price(InstrumentId instrument) const noexcept {
    if (instrument >= MAX_INSTRUMENTS) return 0.0;
    return avg_prices_[instrument];
}

double PositionTracker::capital_used() const noexcept {
    double capital = 0.0;
    for (size_t i = 0; i < MAX_INSTRUMENTS; ++i) {
        if (positions_[i] != 0) {
            double price = (mark_prices_[i] > 0)
                ? static_cast<double>(mark_prices_[i]) / PRICE_SCALE
                : avg_prices_[i];
            capital += std::abs(static_cast<double>(positions_[i])) * price;
        }
    }
    return capital;
}

} // namespace trading
