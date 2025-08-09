#include <benchmark/benchmark.h>
#include "order_book/order_book.hpp"
#include "market_data/feed_simulator.hpp"
#include "market_data/market_data_handler.hpp"
#include "execution/execution_engine.hpp"

using namespace trading;

static void BM_OrderBookThroughput(benchmark::State& state) {
    OrderBook book(0);
    OrderId id = 1;

    for (auto _ : state) {
        // Add and match — simulates book update
        book.add_order(id, Side::Sell, OrderType::Limit, 15000, 100, 0);
        book.add_order(id + 1, Side::Buy, OrderType::Limit, 15000, 100, 0);
        id += 2;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookThroughput);

static void BM_MarketDataThroughput(benchmark::State& state) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);

    MarketDataMessage md;
    for (auto _ : state) {
        auto msg = feed.next_message();
        handler.process_message(msg);
        queue.try_pop(md);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MarketDataThroughput);

static void BM_OrderSubmissionThroughput(benchmark::State& state) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 0, 1.0, true}); // Zero latency
    engine.seed_books(15000, 20, 100000);
    engine.set_rate_limit(10000000);

    OrderId id = 1;
    for (auto _ : state) {
        OrderRequest req{};
        req.id = id++;
        req.instrument = 0;
        req.side = (id % 2 == 0) ? Side::Buy : Side::Sell;
        req.type = OrderType::Limit;
        req.price = 15000;
        req.quantity = 10;
        req.timestamp = 0;
        engine.process_order(req);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderSubmissionThroughput);

BENCHMARK_MAIN();
