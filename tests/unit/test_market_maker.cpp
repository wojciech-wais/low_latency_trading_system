#include <gtest/gtest.h>
#include "strategy/market_maker.hpp"

using namespace trading;

class MarketMakerTest : public ::testing::Test {
protected:
    MarketMakerStrategy::Params params_;
    void SetUp() override {
        params_.base_spread_bps = 10.0;
        params_.max_inventory = 100;
        params_.order_size = 10;
        params_.skew_factor = 0.5;
        params_.instrument = 0;
    }

    MarketDataMessage make_md(Price bid, Price ask) {
        MarketDataMessage md{};
        md.instrument = 0;
        md.bid_price = bid;
        md.ask_price = ask;
        md.bid_quantity = 100;
        md.ask_quantity = 100;
        md.last_price = (bid + ask) / 2;
        md.last_quantity = 50;
        md.timestamp = now_ns();
        md.msg_type = 'W';
        return md;
    }
};

TEST_F(MarketMakerTest, SymmetricQuotes) {
    MarketMakerStrategy mm(params_);
    mm.on_market_data(make_md(15000, 15010));

    auto orders = mm.generate_orders();
    ASSERT_EQ(orders.size(), 2u);

    // Should have one buy and one sell
    bool has_buy = false, has_sell = false;
    for (const auto& o : orders) {
        if (o.side == Side::Buy) has_buy = true;
        if (o.side == Side::Sell) has_sell = true;
        EXPECT_EQ(o.instrument, 0u);
        EXPECT_EQ(o.quantity, 10u);
    }
    EXPECT_TRUE(has_buy);
    EXPECT_TRUE(has_sell);
}

TEST_F(MarketMakerTest, InventorySkew) {
    MarketMakerStrategy mm(params_);
    mm.on_market_data(make_md(15000, 15010));

    // Simulate buying inventory
    ExecutionReport fill{};
    fill.instrument = 0;
    fill.side = Side::Buy;
    fill.status = OrderStatus::Filled;
    fill.filled_quantity = 50;
    mm.on_execution_report(fill);

    EXPECT_EQ(mm.inventory(), 50);

    auto orders = mm.generate_orders();
    ASSERT_EQ(orders.size(), 2u);

    // With positive inventory, bid should be lower (less aggressive buying)
    Price bid = 0, ask = 0;
    for (const auto& o : orders) {
        if (o.side == Side::Buy) bid = o.price;
        if (o.side == Side::Sell) ask = o.price;
    }
    EXPECT_GT(ask, bid);
}

TEST_F(MarketMakerTest, MaxInventoryFlatten) {
    MarketMakerStrategy mm(params_);
    mm.on_market_data(make_md(15000, 15010));

    // Fill to max inventory
    ExecutionReport fill{};
    fill.instrument = 0;
    fill.side = Side::Buy;
    fill.status = OrderStatus::Filled;
    fill.filled_quantity = 100;
    mm.on_execution_report(fill);

    EXPECT_EQ(mm.inventory(), 100);

    auto orders = mm.generate_orders();
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].side, Side::Sell); // Aggressive sell to flatten
}

TEST_F(MarketMakerTest, NoBBONoOrders) {
    MarketMakerStrategy mm(params_);
    auto orders = mm.generate_orders();
    EXPECT_TRUE(orders.empty());
}

TEST_F(MarketMakerTest, VolatilityWidensSpread) {
    MarketMakerStrategy mm(params_);

    // Feed stable prices to establish baseline
    for (int i = 0; i < 20; ++i) {
        mm.on_market_data(make_md(15000, 15010));
    }
    double spread1 = mm.current_spread_bps();

    // Feed volatile prices
    MarketMakerStrategy mm2(params_);
    for (int i = 0; i < 20; ++i) {
        Price offset = (i % 2 == 0) ? 100 : -100;
        mm2.on_market_data(make_md(15000 + offset, 15010 + offset));
    }
    double spread2 = mm2.current_spread_bps();

    EXPECT_GT(spread2, spread1);
}
