#pragma once

#include "common/types.hpp"

namespace trading {

/// OrderBookEntry extends Order with intrusive doubly-linked list pointers
/// for O(1) removal from a price level. NOT cache-line sized — used only
/// inside the order book engine, not transported over queues.
struct OrderBookEntry {
    OrderId id;
    InstrumentId instrument;
    Side side;
    OrderType type;
    OrderStatus status;
    Price price;
    Quantity quantity;
    Quantity filled_quantity;
    Timestamp timestamp;

    // Intrusive doubly-linked list for price level
    OrderBookEntry* prev = nullptr;
    OrderBookEntry* next = nullptr;
};

} // namespace trading
