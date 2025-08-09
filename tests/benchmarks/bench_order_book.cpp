#include <benchmark/benchmark.h>
#include "order_book/order_book.hpp"

using namespace trading;

static void BM_OrderBookAddNewLevel(benchmark::State& state) {
    OrderBook book(0);
    OrderId id = 1;
    for (auto _ : state) {
        Price price = 15000 + static_cast<Price>(id % 1000);
        book.add_order(id++, Side::Buy, OrderType::Limit, price, 100, 0);
        // Cancel to keep book clean
        book.cancel_order(id - 1);
    }
}
BENCHMARK(BM_OrderBookAddNewLevel);

static void BM_OrderBookAddExistingLevel(benchmark::State& state) {
    OrderBook book(0);
    // Seed some levels
    for (int i = 0; i < 10; ++i) {
        book.add_order(900000 + i, Side::Buy, OrderType::Limit, 15000, 100, 0);
    }
    OrderId id = 1;
    for (auto _ : state) {
        book.add_order(id, Side::Buy, OrderType::Limit, 15000, 100, 0);
        book.cancel_order(id);
        ++id;
    }
}
BENCHMARK(BM_OrderBookAddExistingLevel);

static void BM_OrderBookCancel(benchmark::State& state) {
    OrderBook book(0);
    OrderId id = 1;
    // Pre-populate
    for (int i = 0; i < 10000; ++i) {
        book.add_order(id++, Side::Buy, OrderType::Limit, 15000 - (i % 100), 100, 0);
    }
    // Benchmark cancel + re-add
    OrderId cancel_id = 1;
    for (auto _ : state) {
        book.cancel_order(cancel_id);
        book.add_order(cancel_id, Side::Buy, OrderType::Limit, 15000, 100, 0);
        ++cancel_id;
        if (cancel_id > 10000) cancel_id = 1;
    }
}
BENCHMARK(BM_OrderBookCancel);

static void BM_OrderBookMatch(benchmark::State& state) {
    OrderBook book(0);
    OrderId id = 1;
    for (auto _ : state) {
        state.PauseTiming();
        // Add resting sell order
        book.add_order(id++, Side::Sell, OrderType::Limit, 15000, 100, 0);
        state.ResumeTiming();
        // Match with buy
        book.add_order(id++, Side::Buy, OrderType::Limit, 15000, 100, 0);
    }
}
BENCHMARK(BM_OrderBookMatch);

static void BM_OrderBookBBO(benchmark::State& state) {
    OrderBook book(0);
    for (int i = 0; i < 100; ++i) {
        book.add_order(i + 1, Side::Buy, OrderType::Limit, 15000 - i, 100, 0);
        book.add_order(10000 + i, Side::Sell, OrderType::Limit, 15100 + i, 100, 0);
    }
    for (auto _ : state) {
        auto bid = book.best_bid();
        auto ask = book.best_ask();
        benchmark::DoNotOptimize(bid);
        benchmark::DoNotOptimize(ask);
    }
}
BENCHMARK(BM_OrderBookBBO);

BENCHMARK_MAIN();
