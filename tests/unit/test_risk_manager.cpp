#include <gtest/gtest.h>
#include "risk/risk_manager.hpp"

using namespace trading;

class RiskManagerTest : public ::testing::Test {
protected:
    RiskLimits limits_;
    void SetUp() override {
        limits_.max_position_per_instrument = 1000;
        limits_.max_total_position = 5000;
        limits_.max_capital = 1'000'000.0;
        limits_.max_order_size = 500;
        limits_.max_orders_per_second = 100;
        limits_.max_price_deviation_pct = 5.0;
        limits_.max_drawdown_pct = 2.0;
    }

    OrderRequest make_order(Quantity qty = 10, Price price = 15000, Side side = Side::Buy) {
        OrderRequest req{};
        req.id = 1;
        req.instrument = 0;
        req.side = side;
        req.type = OrderType::Limit;
        req.price = price;
        req.quantity = qty;
        req.timestamp = now_ns();
        return req;
    }
};

TEST_F(RiskManagerTest, Approved) {
    RiskManager mgr(limits_);
    auto req = make_order(10, 15000);
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::Approved);
}

TEST_F(RiskManagerTest, KillSwitch) {
    RiskManager mgr(limits_);
    mgr.activate_kill_switch();
    EXPECT_TRUE(mgr.kill_switch_active());

    auto req = make_order();
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::KillSwitchActive);

    mgr.deactivate_kill_switch();
    EXPECT_FALSE(mgr.kill_switch_active());
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::Approved);
}

TEST_F(RiskManagerTest, OrderSizeTooLarge) {
    RiskManager mgr(limits_);
    auto req = make_order(600); // Limit is 500
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::OrderSizeTooLarge);
}

TEST_F(RiskManagerTest, PositionLimitBreached) {
    RiskManager mgr(limits_);

    // Simulate existing position
    mgr.position_tracker().on_fill(0, Side::Buy, 990, 15000);

    auto req = make_order(20); // Would make position 1010 > limit 1000
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::PositionLimitBreached);
}

TEST_F(RiskManagerTest, CapitalLimitBreached) {
    limits_.max_capital = 100.0; // Very low capital limit
    RiskManager mgr(limits_);

    auto req = make_order(100, 15000); // 100 * $150 = $15,000
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::CapitalLimitBreached);
}

TEST_F(RiskManagerTest, OrderRateExceeded) {
    limits_.max_orders_per_second = 5;
    RiskManager mgr(limits_);

    auto req = make_order();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::Approved);
    }
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::OrderRateExceeded);
}

TEST_F(RiskManagerTest, FatFingerPrice) {
    RiskManager mgr(limits_);

    // Order price 10% away from market (limit is 5%)
    auto req = make_order(10, 16500); // 10% above 15000
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::FatFingerPrice);
}

TEST_F(RiskManagerTest, FatFingerApproved) {
    RiskManager mgr(limits_);

    // Order price 2% away (within 5% limit)
    auto req = make_order(10, 15300); // 2% above 15000
    EXPECT_EQ(mgr.check_order(req, 15000), RiskCheckResult::Approved);
}

TEST_F(RiskManagerTest, DrawdownAutoKill) {
    RiskManager mgr(limits_);
    mgr.set_peak_pnl(1000.0);

    // 3% drawdown (> 2% limit)
    mgr.on_pnl_update(970.0);
    EXPECT_TRUE(mgr.kill_switch_active());
}

TEST_F(RiskManagerTest, ChecksStats) {
    RiskManager mgr(limits_);
    auto req = make_order();
    mgr.check_order(req, 15000);
    mgr.check_order(req, 15000);

    EXPECT_EQ(mgr.checks_performed(), 2u);
}
