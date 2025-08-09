#pragma once

#include "order_book/order.hpp"

namespace trading {

/// PriceLevel: a FIFO queue of orders at the same price.
/// Uses intrusive doubly-linked list for O(1) add/remove.
struct PriceLevel {
    Price price = 0;
    Quantity total_quantity = 0;
    uint32_t order_count = 0;

    // Intrusive list head/tail
    OrderBookEntry* head = nullptr;
    OrderBookEntry* tail = nullptr;

    /// Add order to back of the level (FIFO / time priority).
    void add_order(OrderBookEntry* entry) noexcept {
        entry->prev = tail;
        entry->next = nullptr;
        if (tail) {
            tail->next = entry;
        } else {
            head = entry;
        }
        tail = entry;
        total_quantity += entry->quantity - entry->filled_quantity;
        ++order_count;
    }

    /// Remove order from level in O(1) by unlinking.
    void remove_order(OrderBookEntry* entry) noexcept {
        if (entry->prev) {
            entry->prev->next = entry->next;
        } else {
            head = entry->next;
        }
        if (entry->next) {
            entry->next->prev = entry->prev;
        } else {
            tail = entry->prev;
        }
        entry->prev = nullptr;
        entry->next = nullptr;
        Quantity remaining = entry->quantity - entry->filled_quantity;
        total_quantity = (total_quantity >= remaining) ? total_quantity - remaining : 0;
        --order_count;
    }

    /// Front of the queue (highest time priority).
    OrderBookEntry* front() const noexcept { return head; }

    bool empty() const noexcept { return head == nullptr; }
};

} // namespace trading
