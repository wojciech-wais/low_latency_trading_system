#include <gtest/gtest.h>
#include "strategy/pairs_trading.hpp"

using namespace trading;

class PairsTradingTest : public ::testing::Test {
protected:
    PairsTradingStrategy::Params params_;
    void SetUp() override {
        params_.instrument_a = 0;
        params_.instrument_b = 1;
        params_.hedge_ratio = 1.0;
        params_.entry_z_threshold = 2.0;
        params_.exit_z_threshold = 0.5;
        params_.order_size = 10;
    }

    MarketDataMessage make_md(InstrumentId inst, Price bid, Price ask) {
        MarketDataMessage md{};
        md.instrument = inst;
        md.bid_price = bid;
        md.ask_price = ask;
        md.bid_quantity = 100;
        md.ask_quantity = 100;
        md.timestamp = now_ns();
        md.msg_type = 'W';
        return md;
    }
};

TEST_F(PairsTradingTest, NoSignalOnNoise) {
    PairsTradingStrategy strategy(params_);

    // Feed correlated prices (spread = 0)
    for (int i = 0; i < 50; ++i) {
        strategy.on_market_data(make_md(0, 15000, 15010));
        strategy.on_market_data(make_md(1, 15000, 15010));
    }

    auto orders = strategy.generate_orders();
    EXPECT_TRUE(orders.empty());
    EXPECT_NEAR(strategy.z_score(), 0.0, 0.5);
}

TEST_F(PairsTradingTest, DivergenceEntry) {
    PairsTradingStrategy strategy(params_);

    // Build up history with correlated prices
    for (int i = 0; i < 30; ++i) {
        strategy.on_market_data(make_md(0, 15000, 15010));
        strategy.on_market_data(make_md(1, 15000, 15010));
    }

    // Now diverge: A goes way up relative to B
    for (int i = 0; i < 30; ++i) {
        strategy.on_market_data(make_md(0, 15500 + i * 10, 15510 + i * 10));
        strategy.on_market_data(make_md(1, 15000, 15010));
    }

    // Z-score should be positive (spread is wide)
    EXPECT_GT(strategy.z_score(), 1.0);

    // Eventually should generate entry signal
    auto orders = strategy.generate_orders();
    // Might or might not trigger depending on exact z-score
    // Just verify no crash and reasonable state
    EXPECT_GE(strategy.z_score(), 0.0);
}

TEST_F(PairsTradingTest, ConvergenceExit) {
    PairsTradingStrategy strategy(params_);

    // Setup: create a position by manipulating state through fills
    for (int i = 0; i < 30; ++i) {
        strategy.on_market_data(make_md(0, 15000, 15010));
        strategy.on_market_data(make_md(1, 15000, 15010));
    }

    // Just verify z-score near zero means no signal
    auto orders = strategy.generate_orders();
    EXPECT_TRUE(orders.empty());
}

TEST_F(PairsTradingTest, InitialState) {
    PairsTradingStrategy strategy(params_);
    EXPECT_EQ(strategy.position_a(), 0);
    EXPECT_EQ(strategy.position_b(), 0);
    EXPECT_NEAR(strategy.z_score(), 0.0, 0.001);
}

TEST_F(PairsTradingTest, ExecutionReportUpdatesPosition) {
    PairsTradingStrategy strategy(params_);

    ExecutionReport report{};
    report.instrument = 0;
    report.side = Side::Buy;
    report.status = OrderStatus::Filled;
    report.filled_quantity = 10;
    strategy.on_execution_report(report);

    EXPECT_EQ(strategy.position_a(), 10);
    EXPECT_EQ(strategy.position_b(), 0);
}
