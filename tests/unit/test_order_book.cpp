#include <gtest/gtest.h>
#include "order_book/order_book.hpp"
#include <random>

using namespace trading;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book_{0};
    OrderId next_id_ = 1;

    OrderId add_limit(Side side, Price price, Quantity qty) {
        OrderId id = next_id_++;
        book_.add_order(id, side, OrderType::Limit, price, qty, now_ns());
        return id;
    }
};

TEST_F(OrderBookTest, EmptyBook) {
    EXPECT_EQ(book_.best_bid(), 0);
    EXPECT_EQ(book_.best_ask(), 0);
    EXPECT_EQ(book_.order_count(), 0u);
    EXPECT_EQ(book_.spread(), 0);
}

TEST_F(OrderBookTest, AddSingleBid) {
    add_limit(Side::Buy, 10000, 100);
    EXPECT_EQ(book_.best_bid(), 10000);
    EXPECT_EQ(book_.best_bid_quantity(), 100u);
    EXPECT_EQ(book_.order_count(), 1u);
}

TEST_F(OrderBookTest, AddSingleAsk) {
    add_limit(Side::Sell, 10100, 50);
    EXPECT_EQ(book_.best_ask(), 10100);
    EXPECT_EQ(book_.best_ask_quantity(), 50u);
}

TEST_F(OrderBookTest, SimpleMatch) {
    add_limit(Side::Sell, 10000, 100);
    auto trades = book_.add_order(2, Side::Buy, OrderType::Limit, 10000, 100, now_ns());
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 10000);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(book_.order_count(), 0u);
}

TEST_F(OrderBookTest, PartialFill) {
    add_limit(Side::Sell, 10000, 100);
    auto trades = book_.add_order(2, Side::Buy, OrderType::Limit, 10000, 50, now_ns());
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_EQ(book_.order_count(), 1u); // Remaining resting order
    EXPECT_EQ(book_.best_ask_quantity(), 50u);
}

TEST_F(OrderBookTest, PriceTimePriority) {
    add_limit(Side::Sell, 10000, 50); // First at 10000
    add_limit(Side::Sell, 10000, 30); // Second at 10000
    add_limit(Side::Sell, 9900, 20);  // Better price

    // Buyer should match at 9900 first, then 10000
    auto trades = book_.add_order(next_id_++, Side::Buy, OrderType::Limit, 10000, 100, now_ns());
    ASSERT_GE(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, 9900);
    EXPECT_EQ(trades[0].quantity, 20u);
    EXPECT_EQ(trades[1].price, 10000);
    EXPECT_EQ(trades[1].quantity, 50u);
}

TEST_F(OrderBookTest, CancelOrder) {
    OrderId id = add_limit(Side::Buy, 10000, 100);
    EXPECT_EQ(book_.order_count(), 1u);
    EXPECT_TRUE(book_.cancel_order(id));
    EXPECT_EQ(book_.order_count(), 0u);
    EXPECT_EQ(book_.best_bid(), 0);
}

TEST_F(OrderBookTest, CancelNonexistent) {
    EXPECT_FALSE(book_.cancel_order(999));
}

TEST_F(OrderBookTest, ModifyOrder) {
    OrderId id = add_limit(Side::Buy, 10000, 100);
    auto trades = book_.modify_order(id, 10100, 200);
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book_.best_bid(), 10100);
    EXPECT_EQ(book_.best_bid_quantity(), 200u);
}

TEST_F(OrderBookTest, MarketOrder) {
    add_limit(Side::Sell, 10000, 100);
    add_limit(Side::Sell, 10100, 100);
    auto trades = book_.add_order(next_id_++, Side::Buy, OrderType::Market, 0, 150, now_ns());
    EXPECT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[1].quantity, 50u);
}

TEST_F(OrderBookTest, IOCOrder) {
    add_limit(Side::Sell, 10000, 50);
    // IOC for 100 — should fill 50 and cancel remaining
    auto trades = book_.add_order(next_id_++, Side::Buy, OrderType::IOC, 10000, 100, now_ns());
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);
    // IOC order should not rest on book
    EXPECT_EQ(book_.bid_level_count(), 0u);
}

TEST_F(OrderBookTest, FOKOrderFull) {
    add_limit(Side::Sell, 10000, 100);
    auto trades = book_.add_order(next_id_++, Side::Buy, OrderType::FOK, 10000, 100, now_ns());
    EXPECT_EQ(trades.size(), 1u);
}

TEST_F(OrderBookTest, FOKOrderReject) {
    add_limit(Side::Sell, 10000, 50);
    // FOK for 100 but only 50 available — should reject entirely
    auto trades = book_.add_order(next_id_++, Side::Buy, OrderType::FOK, 10000, 100, now_ns());
    EXPECT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, Depth) {
    add_limit(Side::Buy, 10000, 100);
    add_limit(Side::Buy, 9900, 200);
    add_limit(Side::Buy, 9800, 300);
    add_limit(Side::Sell, 10100, 150);
    add_limit(Side::Sell, 10200, 250);

    OrderBook::DepthEntry bids[5], asks[5];
    size_t levels = book_.get_depth(bids, asks, 5);

    EXPECT_GE(levels, 2u);
    EXPECT_EQ(bids[0].price, 10000);
    EXPECT_EQ(bids[0].quantity, 100u);
    EXPECT_EQ(bids[1].price, 9900);
    EXPECT_EQ(asks[0].price, 10100);
    EXPECT_EQ(asks[0].quantity, 150u);
}

TEST_F(OrderBookTest, VWAP) {
    add_limit(Side::Buy, 10000, 100);
    add_limit(Side::Buy, 9900, 200);

    double vwap = book_.vwap(Side::Buy, 2);
    // VWAP = (10000*100 + 9900*200) / (100+200) = (1000000+1980000)/300 = 9933.33
    EXPECT_NEAR(vwap, 9933.33, 1.0);
}

TEST_F(OrderBookTest, Spread) {
    add_limit(Side::Buy, 10000, 100);
    add_limit(Side::Sell, 10100, 100);
    EXPECT_EQ(book_.spread(), 100);
}

TEST_F(OrderBookTest, StressTest) {
    // Add and cancel many orders
    std::mt19937 rng(42);
    std::uniform_int_distribution<Price> price_dist(9000, 11000);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<Quantity> qty_dist(1, 100);

    std::vector<OrderId> order_ids;

    for (int i = 0; i < 100000; ++i) {
        Side side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
        Price price = price_dist(rng);
        Quantity qty = qty_dist(rng);

        OrderId id = next_id_++;
        book_.add_order(id, side, OrderType::Limit, price, qty, now_ns());
        order_ids.push_back(id);

        // Randomly cancel some
        if (order_ids.size() > 100 && rng() % 3 == 0) {
            size_t idx = rng() % order_ids.size();
            book_.cancel_order(order_ids[idx]);
        }
    }

    // Should not crash and BBO should be valid
    EXPECT_GE(book_.best_bid(), 0);
}
