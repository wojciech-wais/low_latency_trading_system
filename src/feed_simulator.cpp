#include "market_data/feed_simulator.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace trading {

FeedSimulator::FeedSimulator()
    : rng_(42) // Deterministic seed for reproducibility
{}

void FeedSimulator::add_instrument(InstrumentId id, const std::string& symbol,
                                    double initial_price, double volatility,
                                    double spread, Quantity base_size) {
    instruments_.push_back({id, symbol, initial_price, volatility, spread, base_size});
}

std::string_view FeedSimulator::next_message() {
    if (instruments_.empty()) return {};

    // Round-robin across instruments
    InstrumentState& state = instruments_[current_instrument_];
    current_instrument_ = (current_instrument_ + 1) % instruments_.size();

    // Random walk: mid_price += volatility * normal_random
    double move = state.volatility * state.mid_price * normal_dist_(rng_);
    state.mid_price += move;
    if (state.mid_price < 0.01) state.mid_price = 0.01; // Floor

    // Vary spread slightly
    double spread_noise = 1.0 + 0.1 * normal_dist_(rng_);
    if (spread_noise < 0.5) spread_noise = 0.5;
    if (spread_noise > 2.0) spread_noise = 2.0;
    (void)spread_noise; // Spread variation applied in build_fix_message

    build_fix_message(state);
    ++msg_count_;

    return std::string_view(msg_buffer_.data(), msg_len_);
}

void FeedSimulator::build_fix_message(const InstrumentState& state) {
    double half_spread = state.spread / 2.0;
    double bid = state.mid_price - half_spread;
    double ask = state.mid_price + half_spread;
    double last = state.mid_price + (state.spread * 0.1 * normal_dist_(rng_));

    // Vary quantity
    int qty_factor = static_cast<int>(1.0 + std::abs(normal_dist_(rng_)));
    Quantity bid_qty = state.base_size * static_cast<Quantity>(qty_factor);
    Quantity ask_qty = state.base_size * static_cast<Quantity>(qty_factor);
    Quantity last_qty = state.base_size / 2;

    // Build FIX message: 8=FIX.4.4|35=W|55=SYMBOL|132=bid|133=ask|134=bid_qty|135=ask_qty|44=last|38=last_qty|
    int len = snprintf(msg_buffer_.data(), MSG_BUFFER_SIZE,
        "8=FIX.4.4|9=200|35=W|49=FEED|56=CLIENT|34=%lu|"
        "55=%s|132=%.2f|133=%.2f|134=%lu|135=%lu|44=%.2f|38=%lu|10=000|",
        msg_count_,
        state.symbol.c_str(),
        bid, ask,
        static_cast<unsigned long>(bid_qty),
        static_cast<unsigned long>(ask_qty),
        last,
        static_cast<unsigned long>(last_qty));

    msg_len_ = (len > 0 && len < static_cast<int>(MSG_BUFFER_SIZE)) ? static_cast<size_t>(len) : 0;
}

bool FeedSimulator::load_csv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    csv_messages_.clear();
    std::string line;

    // Skip header
    if (!std::getline(file, line)) return false;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Parse CSV: timestamp,instrument,bid,ask,bid_qty,ask_qty,last,last_qty
        std::istringstream ss(line);
        std::string timestamp, instrument, bid, ask, bid_qty, ask_qty, last, last_qty;
        std::getline(ss, timestamp, ',');
        std::getline(ss, instrument, ',');
        std::getline(ss, bid, ',');
        std::getline(ss, ask, ',');
        std::getline(ss, bid_qty, ',');
        std::getline(ss, ask_qty, ',');
        std::getline(ss, last, ',');
        std::getline(ss, last_qty, ',');

        // Convert to FIX message
        char buf[512];
        int len = snprintf(buf, sizeof(buf),
            "8=FIX.4.4|9=200|35=W|49=FEED|56=CLIENT|34=%zu|"
            "55=%s|132=%s|133=%s|134=%s|135=%s|44=%s|38=%s|10=000|",
            csv_messages_.size() + 1,
            instrument.c_str(),
            bid.c_str(), ask.c_str(),
            bid_qty.c_str(), ask_qty.c_str(),
            last.c_str(), last_qty.c_str());

        if (len > 0) {
            csv_messages_.emplace_back(buf, static_cast<size_t>(len));
        }
    }

    csv_pos_ = 0;
    return !csv_messages_.empty();
}

std::string_view FeedSimulator::next_csv_message() {
    if (csv_pos_ >= csv_messages_.size()) return {};
    return csv_messages_[csv_pos_++];
}

void FeedSimulator::reset_csv() {
    csv_pos_ = 0;
}

} // namespace trading
