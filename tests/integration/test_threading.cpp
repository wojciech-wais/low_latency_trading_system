#include <gtest/gtest.h>
#include "common/types.hpp"
#include "containers/lock_free_queue.hpp"
#include "market_data/feed_simulator.hpp"
#include "market_data/market_data_handler.hpp"
#include "execution/execution_engine.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace trading;

TEST(ThreadingTest, MarketDataHandlerThread) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);

    std::atomic<bool> stop{false};
    handler.start(0, [&]() -> std::string_view {
        if (stop.load(std::memory_order_relaxed)) return {};
        return feed.next_message();
    });

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);
    handler.stop();

    EXPECT_GT(handler.messages_processed(), 0u);

    // Drain queue
    MarketDataMessage md;
    uint64_t count = 0;
    while (queue.try_pop(md)) ++count;
    EXPECT_GT(count, 0u);
}

TEST(ThreadingTest, ExecutionEngineThread) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});

    engine.start(0);

    // Push some orders
    for (int i = 0; i < 100; ++i) {
        OrderRequest req{};
        req.id = static_cast<OrderId>(i + 1);
        req.instrument = 0;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.price = 15000;
        req.quantity = 10;
        req.timestamp = now_ns();
        input.try_push(req);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.stop();

    // Should have processed all orders
    ExecutionReport report;
    uint64_t count = 0;
    while (output.try_pop(report)) ++count;
    EXPECT_EQ(count, 100u);
}

TEST(ThreadingTest, MultiThreadCleanShutdown) {
    MarketDataHandler::OutputQueue md_queue;
    ExecutionEngine::InputQueue order_queue;
    ExecutionEngine::OutputQueue exec_queue;

    MarketDataHandler md_handler(md_queue);
    ExecutionEngine exec_engine(order_queue, exec_queue);
    exec_engine.add_exchange({0, "TEST", 100, 1.0, true});

    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00);

    std::atomic<bool> stop{false};
    md_handler.start(0, [&]() -> std::string_view {
        if (stop.load(std::memory_order_relaxed)) return {};
        return feed.next_message();
    });
    exec_engine.start(1);

    // Run under load for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Clean shutdown
    stop.store(true);
    md_handler.stop();
    exec_engine.stop();

    EXPECT_GT(md_handler.messages_processed(), 0u);
    // Should shutdown cleanly without deadlock
}

TEST(ThreadingTest, QueueConcurrentAccess) {
    LockFreeRingBuffer<uint64_t, 65536> queue;
    constexpr uint64_t NUM_ITEMS = 500000;

    std::thread producer([&]() {
        for (uint64_t i = 0; i < NUM_ITEMS; ++i) {
            while (!queue.try_push(i)) {}
        }
    });

    uint64_t consumed = 0;
    uint64_t last_val = 0;
    bool ordering_ok = true;

    std::thread consumer([&]() {
        uint64_t val;
        while (consumed < NUM_ITEMS) {
            if (queue.try_pop(val)) {
                if (val != consumed) ordering_ok = false;
                last_val = val;
                ++consumed;
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed, NUM_ITEMS);
    EXPECT_TRUE(ordering_ok);
}
