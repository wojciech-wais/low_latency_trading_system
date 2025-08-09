#include <gtest/gtest.h>
#include "strategy/momentum.hpp"

using namespace trading;

class MomentumTest : public ::testing::Test {
protected:
    MomentumStrategy::Params params_;
    void SetUp() override {
        params_.instrument = 0;
        params_.fast_window = 5;
        params_.slow_window = 15;
        params_.breakout_threshold_bps = 5.0;
        params_.order_size = 10;
    }

    MarketDataMessage make_md(Price mid) {
        MarketDataMessage md{};
        md.instrument = 0;
        md.bid_price = mid - 5;
        md.ask_price = mid + 5;
        md.bid_quantity = 100;
        md.ask_quantity = 100;
        md.last_price = mid;
        md.last_quantity = 50;
        md.timestamp = now_ns();
        md.msg_type = 'W';
        return md;
    }
};

TEST_F(MomentumTest, NoSignalOnFlat) {
    MomentumStrategy strategy(params_);

    for (int i = 0; i < 50; ++i) {
        strategy.on_market_data(make_md(15000));
    }

    auto orders = strategy.generate_orders();
    EXPECT_TRUE(orders.empty());
    EXPECT_NEAR(strategy.momentum_signal(), 0.0, 1.0);
}

TEST_F(MomentumTest, TrendEntry) {
    MomentumStrategy strategy(params_);

    // Upward trend
    for (int i = 0; i < 50; ++i) {
        strategy.on_market_data(make_md(15000 + i * 5));
    }

    // Momentum should be positive
    EXPECT_GT(strategy.momentum_signal(), 0.0);
}

TEST_F(MomentumTest, CrossoverExit) {
    MomentumStrategy strategy(params_);

    // First trend up
    for (int i = 0; i < 30; ++i) {
        strategy.on_market_data(make_md(15000 + i * 5));
        strategy.generate_orders();
    }

    // Then trend down (should eventually signal exit)
    for (int i = 0; i < 30; ++i) {
        strategy.on_market_data(make_md(15150 - i * 5));
        strategy.generate_orders();
    }

    // Momentum should have turned negative
    EXPECT_LT(strategy.momentum_signal(), 5.0);
}

TEST_F(MomentumTest, InsufficientData) {
    MomentumStrategy strategy(params_);

    // Only feed a few data points (less than slow_window)
    for (int i = 0; i < 5; ++i) {
        strategy.on_market_data(make_md(15000 + i * 100));
    }

    auto orders = strategy.generate_orders();
    EXPECT_TRUE(orders.empty());
}

TEST_F(MomentumTest, EMAs) {
    MomentumStrategy strategy(params_);

    strategy.on_market_data(make_md(15000));
    EXPECT_NEAR(strategy.fast_ema(), 15000.0, 1.0);
    EXPECT_NEAR(strategy.slow_ema(), 15000.0, 1.0);
}

TEST_F(MomentumTest, PositionTracking) {
    MomentumStrategy strategy(params_);

    ExecutionReport report{};
    report.instrument = 0;
    report.side = Side::Buy;
    report.status = OrderStatus::Filled;
    report.filled_quantity = 10;
    strategy.on_execution_report(report);

    EXPECT_EQ(strategy.position(), 10);
}
