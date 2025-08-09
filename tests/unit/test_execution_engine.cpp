#include <gtest/gtest.h>
#include "execution/execution_engine.hpp"

using namespace trading;

TEST(ExecutionEngineTest, ProcessOrder) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();

    auto report = engine.process_order(req);
    EXPECT_EQ(report.order_id, 1u);
    EXPECT_EQ(engine.orders_processed(), 1u);
}

TEST(ExecutionEngineTest, QueueInOut) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});

    // Push order to input queue
    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 100;
    req.timestamp = now_ns();
    input.try_push(req);

    // Start and stop engine
    engine.start(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    engine.stop();

    // Should have produced execution report
    ExecutionReport report;
    EXPECT_TRUE(output.try_pop(report));
    EXPECT_EQ(report.order_id, 1u);
}

TEST(ExecutionEngineTest, Throttling) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});
    engine.set_rate_limit(5);

    for (int i = 0; i < 10; ++i) {
        OrderRequest req{};
        req.id = static_cast<OrderId>(i + 1);
        req.instrument = 0;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.price = 15000;
        req.quantity = 100;
        req.timestamp = now_ns();

        engine.process_order(req);
    }

    // Some should have been throttled
    EXPECT_GT(engine.orders_throttled(), 0u);
}

TEST(ExecutionEngineTest, SeedBooks) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});
    engine.seed_books(15000, 5, 100);
    // Should not crash
}
