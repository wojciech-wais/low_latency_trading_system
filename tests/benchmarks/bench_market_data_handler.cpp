#include <benchmark/benchmark.h>
#include "market_data/market_data_handler.hpp"
#include "market_data/feed_simulator.hpp"

using namespace trading;

static void BM_MarketDataHandlerProcess(benchmark::State& state) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);

    MarketDataMessage md;
    for (auto _ : state) {
        auto msg = feed.next_message();
        handler.process_message(msg);
        queue.try_pop(md);
        benchmark::DoNotOptimize(md);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MarketDataHandlerProcess);

static void BM_MarketDataEndToEnd(benchmark::State& state) {
    MarketDataHandler::OutputQueue queue;
    MarketDataHandler handler(queue);
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);
    feed.add_instrument(1, "GOOG", 280.00, 0.001, 0.03, 50);

    MarketDataMessage md;
    for (auto _ : state) {
        auto msg = feed.next_message();
        handler.process_message(msg);
        queue.try_pop(md);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MarketDataEndToEnd);

BENCHMARK_MAIN();
