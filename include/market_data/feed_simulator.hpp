#pragma once

#include "common/types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <random>
#include <array>

namespace trading {

/// Synthetic FIX message generator with random walk pricing.
/// Also supports CSV replay mode via pre-loaded data.
class FeedSimulator {
public:
    struct InstrumentState {
        InstrumentId id;
        std::string symbol;
        double mid_price;       // Current mid price (floating-point for simulation)
        double volatility;      // Per-tick volatility
        double spread;          // Current bid-ask spread
        Quantity base_size;     // Base quote size
    };

    FeedSimulator();

    /// Configure instruments
    void add_instrument(InstrumentId id, const std::string& symbol,
                        double initial_price, double volatility = 0.001,
                        double spread = 0.02, Quantity base_size = 100);

    /// Generate next FIX market data message (random walk mode).
    /// Returns string_view into internal buffer.
    std::string_view next_message();

    /// Load CSV file for replay mode
    bool load_csv(const std::string& path);

    /// Get next CSV-based message. Returns empty when exhausted.
    std::string_view next_csv_message();

    /// Reset CSV replay position
    void reset_csv();

    uint64_t messages_generated() const noexcept { return msg_count_; }

private:
    void build_fix_message(const InstrumentState& state);

    std::vector<InstrumentState> instruments_;
    std::mt19937 rng_;
    std::normal_distribution<double> normal_dist_{0.0, 1.0};
    uint64_t msg_count_ = 0;
    size_t current_instrument_ = 0;

    // Pre-allocated message buffer
    static constexpr size_t MSG_BUFFER_SIZE = 512;
    std::array<char, MSG_BUFFER_SIZE> msg_buffer_{};
    size_t msg_len_ = 0;

    // CSV replay
    std::vector<std::string> csv_messages_;
    size_t csv_pos_ = 0;
};

} // namespace trading
