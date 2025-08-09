#pragma once

#include "order_book/price_level.hpp"
#include "containers/memory_pool.hpp"
#include <map>
#include <unordered_map>
#include <array>
#include <span>
#include <functional>

namespace trading {

/// OrderBook: price-time priority matching engine.
/// - Bids: descending price (std::greater)
/// - Asks: ascending price (default)
/// - O(1) order lookup via unordered_map
/// - O(1) cancel via intrusive list
/// - Returns trades via std::span over thread-local static array (no heap alloc)
class OrderBook {
public:
    static constexpr size_t MAX_TRADES_PER_MATCH = 64;
    static constexpr size_t ORDER_POOL_SIZE = 65536;

    explicit OrderBook(InstrumentId instrument = 0);

    /// Add an order. Returns span of trades if matching occurred.
    std::span<Trade> add_order(OrderId id, Side side, OrderType type,
                                Price price, Quantity quantity, Timestamp timestamp);

    /// Cancel an order. Returns true if found and cancelled.
    bool cancel_order(OrderId id);

    /// Modify an order (cancel + re-add). Returns trades if new order matches.
    std::span<Trade> modify_order(OrderId id, Price new_price, Quantity new_quantity);

    /// Best bid/ask (O(1) cached).
    Price best_bid() const noexcept;
    Price best_ask() const noexcept;
    Quantity best_bid_quantity() const noexcept;
    Quantity best_ask_quantity() const noexcept;

    /// Market depth: returns number of levels filled.
    struct DepthEntry {
        Price price;
        Quantity quantity;
        uint32_t order_count;
    };
    size_t get_depth(DepthEntry* bids, DepthEntry* asks, size_t max_levels) const;

    /// VWAP over top N levels on a given side.
    double vwap(Side side, size_t levels) const;

    /// Spread in ticks.
    Price spread() const noexcept;

    /// Stats
    size_t order_count() const noexcept { return orders_.size(); }
    size_t bid_level_count() const noexcept { return bids_.size(); }
    size_t ask_level_count() const noexcept { return asks_.size(); }

    InstrumentId instrument() const noexcept { return instrument_; }

private:
    std::span<Trade> match_order(OrderBookEntry* entry);
    void try_match_limit(OrderBookEntry* entry, size_t& trade_count);
    void try_match_market(OrderBookEntry* entry, size_t& trade_count);
    void add_to_book(OrderBookEntry* entry);
    void remove_from_book(OrderBookEntry* entry);
    void update_best_bid();
    void update_best_ask();

    InstrumentId instrument_;
    MemoryPool<OrderBookEntry, ORDER_POOL_SIZE> pool_;

    // Price level maps
    std::map<Price, PriceLevel, std::greater<Price>> bids_; // Descending
    std::map<Price, PriceLevel> asks_;                       // Ascending

    // O(1) order lookup
    std::unordered_map<OrderId, OrderBookEntry*> orders_;

    // Cached BBO
    Price best_bid_ = 0;
    Price best_ask_ = 0;
    Quantity best_bid_qty_ = 0;
    Quantity best_ask_qty_ = 0;

    // Thread-local trade buffer to avoid heap allocation
    static thread_local std::array<Trade, MAX_TRADES_PER_MATCH> trade_buffer_;
};

} // namespace trading
