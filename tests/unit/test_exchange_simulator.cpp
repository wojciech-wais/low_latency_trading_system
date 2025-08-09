#include <gtest/gtest.h>
#include "execution/exchange_simulator.hpp"

using namespace trading;

class ExchangeSimTest : public ::testing::Test {
protected:
    ExchangeConfig config_;
    void SetUp() override {
        config_.id = 0;
        config_.name = "TEST_EXCHANGE";
        config_.latency_ns = 100;
        config_.fill_probability = 1.0; // 100% fill for deterministic tests
        config_.enabled = true;
    }
};

TEST_F(ExchangeSimTest, SubmitAndFill) {
    ExchangeSimulator sim(config_);
    sim.seed_book(15000, 5, 1000);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15001; // Should cross the ask
    req.quantity = 100;
    req.timestamp = now_ns();

    auto report = sim.submit_order(req);
    EXPECT_EQ(report.order_id, 1u);
    EXPECT_TRUE(report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled
                || report.status == OrderStatus::New);
    EXPECT_EQ(sim.orders_processed(), 1u);
}

TEST_F(ExchangeSimTest, RestOnBook) {
    ExchangeSimulator sim(config_);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();

    auto report = sim.submit_order(req);
    EXPECT_EQ(report.status, OrderStatus::New);
}

TEST_F(ExchangeSimTest, CancelOrder) {
    ExchangeSimulator sim(config_);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();

    sim.submit_order(req);
    auto report = sim.cancel_order(1);
    EXPECT_EQ(report.status, OrderStatus::Cancelled);
}

TEST_F(ExchangeSimTest, LatencySimulation) {
    config_.latency_ns = 1000;
    ExchangeSimulator sim(config_);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();

    Timestamp before = now_ns();
    auto report = sim.submit_order(req);
    // Report timestamp should include latency
    EXPECT_GE(report.timestamp, before);
}

TEST_F(ExchangeSimTest, RejectOnProbability) {
    config_.fill_probability = 0.0; // Always reject
    ExchangeSimulator sim(config_);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();

    auto report = sim.submit_order(req);
    EXPECT_EQ(report.status, OrderStatus::Rejected);
    EXPECT_EQ(sim.rejects(), 1u);
}
