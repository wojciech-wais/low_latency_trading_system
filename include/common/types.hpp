#pragma once

#include <cstdint>
#include <ctime>
#include <type_traits>

namespace trading {

// Core type aliases
using Price = int64_t;          // Fixed-point: 150.50 stored as 15050 (2 decimal places)
using Quantity = uint64_t;
using OrderId = uint64_t;
using InstrumentId = uint32_t;
using ExchangeId = uint8_t;
using Timestamp = uint64_t;     // Nanoseconds since epoch

// Constants
constexpr int PRICE_SCALE = 100;    // 2 decimal places
constexpr size_t MAX_INSTRUMENTS = 256;
constexpr size_t MAX_EXCHANGES = 16;
constexpr size_t CACHE_LINE_SIZE = 64;

// Enums
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1,
    IOC = 2,    // Immediate or Cancel
    FOK = 3     // Fill or Kill
};

enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4
};

// Core structures — Order fits in a single 64-byte cache line
struct alignas(CACHE_LINE_SIZE) Order {
    OrderId id;                 // 8 bytes
    InstrumentId instrument;    // 4 bytes
    Side side;                  // 1 byte
    OrderType type;             // 1 byte
    OrderStatus status;         // 1 byte
    uint8_t padding_;           // 1 byte
    Price price;                // 8 bytes
    Quantity quantity;           // 8 bytes
    Quantity filled_quantity;    // 8 bytes
    Timestamp timestamp;        // 8 bytes
    // Total: 48 bytes, padded to 64 with alignas
};
static_assert(sizeof(Order) == CACHE_LINE_SIZE, "Order must be exactly one cache line");

struct Trade {
    OrderId buyer_order_id;
    OrderId seller_order_id;
    InstrumentId instrument;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
};

struct MarketDataMessage {
    InstrumentId instrument;
    Price bid_price;
    Price ask_price;
    Quantity bid_quantity;
    Quantity ask_quantity;
    Price last_price;
    Quantity last_quantity;
    Timestamp timestamp;
    uint8_t msg_type;   // 'W' = snapshot, 'X' = incremental
    uint8_t padding_[7];
};

struct OrderRequest {
    OrderId id;
    InstrumentId instrument;
    Side side;
    OrderType type;
    Price price;
    Quantity quantity;
    ExchangeId exchange;
    Timestamp timestamp;
};

struct ExecutionReport {
    OrderId order_id;
    OrderId exec_id;
    InstrumentId instrument;
    Side side;
    OrderStatus status;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Quantity leaves_quantity;
    Timestamp timestamp;
    ExchangeId exchange;
};

// Helper functions
constexpr Price to_fixed_price(double price) noexcept {
    return static_cast<Price>(price * PRICE_SCALE + (price >= 0 ? 0.5 : -0.5));
}

constexpr double to_double_price(Price price) noexcept {
    return static_cast<double>(price) / PRICE_SCALE;
}

inline Timestamp now_ns() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<Timestamp>(ts.tv_sec) * 1'000'000'000ULL + static_cast<Timestamp>(ts.tv_nsec);
}

constexpr Side opposite_side(Side s) noexcept {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

} // namespace trading
