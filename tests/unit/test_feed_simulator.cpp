#include <gtest/gtest.h>
#include "market_data/feed_simulator.hpp"
#include "market_data/fix_parser.hpp"

using namespace trading;

TEST(FeedSimulatorTest, GenerateMessages) {
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);

    auto msg = feed.next_message();
    EXPECT_FALSE(msg.empty());
    EXPECT_EQ(feed.messages_generated(), 1u);

    // Should be a valid FIX message
    FixParser parser;
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.msg_type(), "W");
    EXPECT_EQ(parser.get_symbol(), "AAPL");
}

TEST(FeedSimulatorTest, MultipleInstruments) {
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00);
    feed.add_instrument(1, "GOOG", 280.00);

    FixParser parser;

    auto msg1 = feed.next_message();
    parser.parse(msg1);
    std::string sym1(parser.get_symbol());

    auto msg2 = feed.next_message();
    parser.parse(msg2);
    std::string sym2(parser.get_symbol());

    // Should round-robin
    EXPECT_NE(sym1, sym2);
}

TEST(FeedSimulatorTest, RandomWalkSanity) {
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001);

    FixParser parser;
    double prev_mid = 0;

    for (int i = 0; i < 1000; ++i) {
        auto msg = feed.next_message();
        ASSERT_FALSE(msg.empty());
        parser.parse(msg);

        double bid = static_cast<double>(parser.get_bid_price()) / PRICE_SCALE;
        double ask = static_cast<double>(parser.get_ask_price()) / PRICE_SCALE;

        EXPECT_GT(ask, bid); // Ask > Bid
        EXPECT_GT(bid, 0.0);

        double mid = (bid + ask) / 2.0;
        if (i > 0) {
            // Price shouldn't jump more than 10% in a single tick
            double pct_change = std::abs(mid - prev_mid) / prev_mid;
            EXPECT_LT(pct_change, 0.1);
        }
        prev_mid = mid;
    }
}

TEST(FeedSimulatorTest, NoInstruments) {
    FeedSimulator feed;
    auto msg = feed.next_message();
    EXPECT_TRUE(msg.empty());
}

TEST(FeedSimulatorTest, MessageCount) {
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00);

    for (int i = 0; i < 100; ++i) {
        feed.next_message();
    }
    EXPECT_EQ(feed.messages_generated(), 100u);
}
