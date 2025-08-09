#include <gtest/gtest.h>
#include "risk/position_tracker.hpp"

using namespace trading;

TEST(PositionTrackerTest, InitialZero) {
    PositionTracker tracker;
    EXPECT_EQ(tracker.position(0), 0);
    EXPECT_EQ(tracker.position(1), 0);
    EXPECT_NEAR(tracker.realized_pnl(), 0.0, 0.001);
    EXPECT_NEAR(tracker.total_pnl(), 0.0, 0.001);
}

TEST(PositionTrackerTest, BuyFill) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000); // Buy 100 @ $150.00
    EXPECT_EQ(tracker.position(0), 100);
    EXPECT_NEAR(tracker.avg_price(0), 150.0, 0.01);
}

TEST(PositionTrackerTest, SellFill) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Sell, 50, 15000);
    EXPECT_EQ(tracker.position(0), -50);
}

TEST(PositionTrackerTest, RealizedPnL) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000); // Buy @ $150.00
    tracker.on_fill(0, Side::Sell, 100, 15100); // Sell @ $151.00
    EXPECT_EQ(tracker.position(0), 0);
    // P&L = 100 * (151.00 - 150.00) = $100
    EXPECT_NEAR(tracker.realized_pnl(), 100.0, 0.01);
}

TEST(PositionTrackerTest, UnrealizedPnL) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000); // Buy @ $150.00
    tracker.update_mark_price(0, 15200);        // Mark @ $152.00
    // Unrealized = 100 * (152.00 - 150.00) = $200
    EXPECT_NEAR(tracker.unrealized_pnl(), 200.0, 0.01);
}

TEST(PositionTrackerTest, TotalAbsolutePosition) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000);
    tracker.on_fill(1, Side::Sell, 50, 28000);
    EXPECT_EQ(tracker.total_absolute_position(), 150);
}

TEST(PositionTrackerTest, CapitalUsed) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000);
    tracker.update_mark_price(0, 15000);
    // Capital = 100 * $150.00 = $15,000
    EXPECT_NEAR(tracker.capital_used(), 15000.0, 1.0);
}

TEST(PositionTrackerTest, Reset) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Buy, 100, 15000);
    tracker.reset();
    EXPECT_EQ(tracker.position(0), 0);
    EXPECT_NEAR(tracker.realized_pnl(), 0.0, 0.001);
}

TEST(PositionTrackerTest, ShortCover) {
    PositionTracker tracker;
    tracker.on_fill(0, Side::Sell, 100, 15100); // Short @ $151.00
    tracker.on_fill(0, Side::Buy, 100, 15000);  // Cover @ $150.00
    EXPECT_EQ(tracker.position(0), 0);
    // P&L = 100 * (151.00 - 150.00) = $100 profit on short
    EXPECT_NEAR(tracker.realized_pnl(), 100.0, 0.01);
}
