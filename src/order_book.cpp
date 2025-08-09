#include "order_book/order_book.hpp"
#include <algorithm>
#include <limits>

namespace trading {

thread_local std::array<Trade, OrderBook::MAX_TRADES_PER_MATCH> OrderBook::trade_buffer_{};

OrderBook::OrderBook(InstrumentId instrument)
    : instrument_(instrument)
    , best_bid_(0)
    , best_ask_(std::numeric_limits<Price>::max())
    , best_bid_qty_(0)
    , best_ask_qty_(0)
{}

std::span<Trade> OrderBook::add_order(OrderId id, Side side, OrderType type,
                                       Price price, Quantity quantity, Timestamp timestamp) {
    OrderBookEntry* entry = pool_.allocate();
    if (!entry) [[unlikely]] {
        return {};
    }

    entry->id = id;
    entry->instrument = instrument_;
    entry->side = side;
    entry->type = type;
    entry->status = OrderStatus::New;
    entry->price = price;
    entry->quantity = quantity;
    entry->filled_quantity = 0;
    entry->timestamp = timestamp;
    entry->prev = nullptr;
    entry->next = nullptr;

    orders_[id] = entry;

    return match_order(entry);
}

std::span<Trade> OrderBook::match_order(OrderBookEntry* entry) {
    size_t trade_count = 0;

    if (entry->type == OrderType::Market) {
        try_match_market(entry, trade_count);
    } else {
        // Limit, IOC, FOK
        try_match_limit(entry, trade_count);
    }

    Quantity remaining = entry->quantity - entry->filled_quantity;

    if (entry->type == OrderType::FOK && remaining > 0 && trade_count > 0) {
        // FOK: must fill completely or not at all — rollback
        // For simplicity in simulation, we just reject
        trade_count = 0;
        entry->filled_quantity = 0;
        entry->status = OrderStatus::Cancelled;
        orders_.erase(entry->id);
        pool_.deallocate(entry);
        return {};
    }

    if (remaining > 0) {
        if (entry->type == OrderType::IOC || entry->type == OrderType::Market) {
            // Cancel remaining
            entry->status = (entry->filled_quantity > 0) ? OrderStatus::PartiallyFilled : OrderStatus::Cancelled;
            orders_.erase(entry->id);
            pool_.deallocate(entry);
        } else if (entry->type == OrderType::Limit) {
            // Rest on book
            entry->status = (entry->filled_quantity > 0) ? OrderStatus::PartiallyFilled : OrderStatus::New;
            add_to_book(entry);
        }
    } else {
        entry->status = OrderStatus::Filled;
        orders_.erase(entry->id);
        pool_.deallocate(entry);
    }

    return std::span<Trade>(trade_buffer_.data(), trade_count);
}

void OrderBook::try_match_limit(OrderBookEntry* entry, size_t& trade_count) {
    auto match_against = [&](auto& levels) {
        auto it = levels.begin();
        while (it != levels.end() && trade_count < MAX_TRADES_PER_MATCH) {
            PriceLevel& level = it->second;

            // Check price compatibility
            if (entry->side == Side::Buy && level.price > entry->price) break;
            if (entry->side == Side::Sell && level.price < entry->price) break;

            while (level.front() && trade_count < MAX_TRADES_PER_MATCH) {
                OrderBookEntry* resting = level.front();
                Quantity entry_remaining = entry->quantity - entry->filled_quantity;
                if (entry_remaining == 0) return;

                Quantity resting_remaining = resting->quantity - resting->filled_quantity;
                Quantity fill_qty = std::min(entry_remaining, resting_remaining);

                // Record trade
                Trade& trade = trade_buffer_[trade_count++];
                trade.buyer_order_id = (entry->side == Side::Buy) ? entry->id : resting->id;
                trade.seller_order_id = (entry->side == Side::Sell) ? entry->id : resting->id;
                trade.instrument = instrument_;
                trade.price = resting->price; // Resting order's price
                trade.quantity = fill_qty;
                trade.timestamp = entry->timestamp;

                entry->filled_quantity += fill_qty;
                resting->filled_quantity += fill_qty;

                if (resting->filled_quantity >= resting->quantity) {
                    resting->status = OrderStatus::Filled;
                    level.remove_order(resting);
                    orders_.erase(resting->id);
                    pool_.deallocate(resting);
                } else {
                    resting->status = OrderStatus::PartiallyFilled;
                    // Update level quantity
                    level.total_quantity -= fill_qty;
                }
            }

            if (level.empty()) {
                it = levels.erase(it);
            } else {
                ++it;
            }
        }
    };

    if (entry->side == Side::Buy) {
        match_against(asks_);
        update_best_ask();
    } else {
        match_against(bids_);
        update_best_bid();
    }
}

void OrderBook::try_match_market(OrderBookEntry* entry, size_t& trade_count) {
    // Market orders match at any price
    if (entry->side == Side::Buy) {
        auto it = asks_.begin();
        while (it != asks_.end() && trade_count < MAX_TRADES_PER_MATCH) {
            PriceLevel& level = it->second;
            while (level.front() && trade_count < MAX_TRADES_PER_MATCH) {
                OrderBookEntry* resting = level.front();
                Quantity entry_remaining = entry->quantity - entry->filled_quantity;
                if (entry_remaining == 0) { update_best_ask(); return; }

                Quantity resting_remaining = resting->quantity - resting->filled_quantity;
                Quantity fill_qty = std::min(entry_remaining, resting_remaining);

                Trade& trade = trade_buffer_[trade_count++];
                trade.buyer_order_id = entry->id;
                trade.seller_order_id = resting->id;
                trade.instrument = instrument_;
                trade.price = resting->price;
                trade.quantity = fill_qty;
                trade.timestamp = entry->timestamp;

                entry->filled_quantity += fill_qty;
                resting->filled_quantity += fill_qty;

                if (resting->filled_quantity >= resting->quantity) {
                    resting->status = OrderStatus::Filled;
                    level.remove_order(resting);
                    orders_.erase(resting->id);
                    pool_.deallocate(resting);
                } else {
                    resting->status = OrderStatus::PartiallyFilled;
                    level.total_quantity -= fill_qty;
                }
            }
            if (level.empty()) {
                it = asks_.erase(it);
            } else {
                ++it;
            }
        }
        update_best_ask();
    } else {
        auto it = bids_.begin();
        while (it != bids_.end() && trade_count < MAX_TRADES_PER_MATCH) {
            PriceLevel& level = it->second;
            while (level.front() && trade_count < MAX_TRADES_PER_MATCH) {
                OrderBookEntry* resting = level.front();
                Quantity entry_remaining = entry->quantity - entry->filled_quantity;
                if (entry_remaining == 0) { update_best_bid(); return; }

                Quantity resting_remaining = resting->quantity - resting->filled_quantity;
                Quantity fill_qty = std::min(entry_remaining, resting_remaining);

                Trade& trade = trade_buffer_[trade_count++];
                trade.buyer_order_id = resting->id;
                trade.seller_order_id = entry->id;
                trade.instrument = instrument_;
                trade.price = resting->price;
                trade.quantity = fill_qty;
                trade.timestamp = entry->timestamp;

                entry->filled_quantity += fill_qty;
                resting->filled_quantity += fill_qty;

                if (resting->filled_quantity >= resting->quantity) {
                    resting->status = OrderStatus::Filled;
                    level.remove_order(resting);
                    orders_.erase(resting->id);
                    pool_.deallocate(resting);
                } else {
                    resting->status = OrderStatus::PartiallyFilled;
                    level.total_quantity -= fill_qty;
                }
            }
            if (level.empty()) {
                it = bids_.erase(it);
            } else {
                ++it;
            }
        }
        update_best_bid();
    }
}

void OrderBook::add_to_book(OrderBookEntry* entry) {
    if (entry->side == Side::Buy) {
        bids_[entry->price].price = entry->price;
        bids_[entry->price].add_order(entry);
        if (entry->price > best_bid_ || best_bid_qty_ == 0) {
            best_bid_ = entry->price;
            best_bid_qty_ = bids_[entry->price].total_quantity;
        }
    } else {
        asks_[entry->price].price = entry->price;
        asks_[entry->price].add_order(entry);
        if (entry->price < best_ask_ || best_ask_qty_ == 0) {
            best_ask_ = entry->price;
            best_ask_qty_ = asks_[entry->price].total_quantity;
        }
    }
}

void OrderBook::remove_from_book(OrderBookEntry* entry) {
    if (entry->side == Side::Buy) {
        auto it = bids_.find(entry->price);
        if (it != bids_.end()) {
            it->second.remove_order(entry);
            if (it->second.empty()) {
                bids_.erase(it);
            }
        }
        update_best_bid();
    } else {
        auto it = asks_.find(entry->price);
        if (it != asks_.end()) {
            it->second.remove_order(entry);
            if (it->second.empty()) {
                asks_.erase(it);
            }
        }
        update_best_ask();
    }
}

bool OrderBook::cancel_order(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;

    OrderBookEntry* entry = it->second;
    entry->status = OrderStatus::Cancelled;
    remove_from_book(entry);
    orders_.erase(it);
    pool_.deallocate(entry);
    return true;
}

std::span<Trade> OrderBook::modify_order(OrderId id, Price new_price, Quantity new_quantity) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return {};

    OrderBookEntry* entry = it->second;
    Side side = entry->side;
    OrderType type = entry->type;
    Timestamp ts = entry->timestamp;

    // Remove old order
    remove_from_book(entry);
    orders_.erase(it);
    pool_.deallocate(entry);

    // Re-add with new parameters (loses time priority)
    return add_order(id, side, type, new_price, new_quantity, ts);
}

void OrderBook::update_best_bid() {
    if (bids_.empty()) {
        best_bid_ = 0;
        best_bid_qty_ = 0;
    } else {
        auto it = bids_.begin();
        best_bid_ = it->first;
        best_bid_qty_ = it->second.total_quantity;
    }
}

void OrderBook::update_best_ask() {
    if (asks_.empty()) {
        best_ask_ = std::numeric_limits<Price>::max();
        best_ask_qty_ = 0;
    } else {
        auto it = asks_.begin();
        best_ask_ = it->first;
        best_ask_qty_ = it->second.total_quantity;
    }
}

Price OrderBook::best_bid() const noexcept { return best_bid_; }
Price OrderBook::best_ask() const noexcept {
    return best_ask_ == std::numeric_limits<Price>::max() ? 0 : best_ask_;
}
Quantity OrderBook::best_bid_quantity() const noexcept { return best_bid_qty_; }
Quantity OrderBook::best_ask_quantity() const noexcept { return best_ask_qty_; }

Price OrderBook::spread() const noexcept {
    if (bids_.empty() || asks_.empty()) return 0;
    return best_ask_ - best_bid_;
}

size_t OrderBook::get_depth(DepthEntry* bid_entries, DepthEntry* ask_entries, size_t max_levels) const {
    size_t count = 0;

    auto bit = bids_.begin();
    for (size_t i = 0; i < max_levels && bit != bids_.end(); ++i, ++bit) {
        bid_entries[i].price = bit->first;
        bid_entries[i].quantity = bit->second.total_quantity;
        bid_entries[i].order_count = bit->second.order_count;
        count = i + 1;
    }

    auto ait = asks_.begin();
    for (size_t i = 0; i < max_levels && ait != asks_.end(); ++i, ++ait) {
        ask_entries[i].price = ait->first;
        ask_entries[i].quantity = ait->second.total_quantity;
        ask_entries[i].order_count = ait->second.order_count;
    }

    return count;
}

double OrderBook::vwap(Side side, size_t levels) const {
    double total_value = 0.0;
    double total_qty = 0.0;

    if (side == Side::Buy) {
        auto it = bids_.begin();
        for (size_t i = 0; i < levels && it != bids_.end(); ++i, ++it) {
            double qty = static_cast<double>(it->second.total_quantity);
            total_value += static_cast<double>(it->first) * qty;
            total_qty += qty;
        }
    } else {
        auto it = asks_.begin();
        for (size_t i = 0; i < levels && it != asks_.end(); ++i, ++it) {
            double qty = static_cast<double>(it->second.total_quantity);
            total_value += static_cast<double>(it->first) * qty;
            total_qty += qty;
        }
    }

    return (total_qty > 0.0) ? total_value / total_qty : 0.0;
}

} // namespace trading
