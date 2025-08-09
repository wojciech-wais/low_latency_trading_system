#include <gtest/gtest.h>
#include "execution/order_router.hpp"

using namespace trading;

class OrderRouterTest : public ::testing::Test {
protected:
    ExchangeConfig config1_, config2_;
    void SetUp() override {
        config1_ = {0, "EX_FAST", 100, 1.0, true};
        config2_ = {1, "EX_SLOW", 500, 1.0, true};
    }
};

TEST_F(OrderRouterTest, RoundRobin) {
    ExchangeSimulator sim1(config1_);
    ExchangeSimulator sim2(config2_);

    OrderRouter router;
    router.add_exchange(&sim1);
    router.add_exchange(&sim2);
    router.set_routing_strategy(OrderRouter::RoutingStrategy::RoundRobin);

    OrderRequest req1{};
    req1.id = 1;
    req1.side = Side::Buy;
    req1.type = OrderType::Limit;
    req1.price = 15000;
    req1.quantity = 100;

    OrderRequest req2 = req1;
    req2.id = 2;

    auto r1 = router.route_order(req1);
    auto r2 = router.route_order(req2);

    // Should alternate exchanges
    EXPECT_NE(r1.exchange, r2.exchange);
}

TEST_F(OrderRouterTest, LowestLatency) {
    ExchangeSimulator sim1(config1_); // 100ns
    ExchangeSimulator sim2(config2_); // 500ns

    OrderRouter router;
    router.add_exchange(&sim1);
    router.add_exchange(&sim2);
    router.set_routing_strategy(OrderRouter::RoutingStrategy::LowestLatency);

    OrderRequest req{};
    req.id = 1;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;

    auto report = router.route_order(req);
    EXPECT_EQ(report.exchange, 0u); // Should route to faster exchange
}

TEST_F(OrderRouterTest, CancelRouting) {
    ExchangeSimulator sim1(config1_);
    OrderRouter router;
    router.add_exchange(&sim1);

    OrderRequest req{};
    req.id = 42;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;

    router.route_order(req);
    auto report = router.cancel_order(42);
    EXPECT_EQ(report.status, OrderStatus::Cancelled);
}

TEST_F(OrderRouterTest, NoExchanges) {
    OrderRouter router;
    OrderRequest req{};
    req.id = 1;
    auto report = router.route_order(req);
    EXPECT_EQ(report.status, OrderStatus::Rejected);
}
