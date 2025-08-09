#include <benchmark/benchmark.h>
#include "strategy/market_maker.hpp"
#include "strategy/pairs_trading.hpp"
#include "strategy/momentum.hpp"

using namespace trading;

static MarketDataMessage make_md(InstrumentId inst, Price bid, Price ask) {
    MarketDataMessage md{};
    md.instrument = inst;
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

static void BM_MarketMakerSignal(benchmark::State& state) {
    MarketMakerStrategy::Params params;
    params.instrument = 0;
    MarketMakerStrategy mm(params);

    // Warm up
    for (int i = 0; i < 100; ++i) {
        mm.on_market_data(make_md(0, 15000, 15010));
    }

    for (auto _ : state) {
        mm.on_market_data(make_md(0, 15000, 15010));
        auto orders = mm.generate_orders();
        benchmark::DoNotOptimize(orders.data());
    }
}
BENCHMARK(BM_MarketMakerSignal);

static void BM_PairsTradingSignal(benchmark::State& state) {
    PairsTradingStrategy::Params params;
    PairsTradingStrategy strategy(params);

    for (int i = 0; i < 100; ++i) {
        strategy.on_market_data(make_md(0, 15000, 15010));
        strategy.on_market_data(make_md(1, 15000, 15010));
    }

    for (auto _ : state) {
        strategy.on_market_data(make_md(0, 15000, 15010));
        strategy.on_market_data(make_md(1, 15000, 15010));
        auto orders = strategy.generate_orders();
        benchmark::DoNotOptimize(orders.data());
    }
}
BENCHMARK(BM_PairsTradingSignal);

static void BM_MomentumSignal(benchmark::State& state) {
    MomentumStrategy::Params params;
    params.instrument = 0;
    MomentumStrategy strategy(params);

    for (int i = 0; i < 50; ++i) {
        strategy.on_market_data(make_md(0, 15000 + i, 15010 + i));
    }

    int tick = 0;
    for (auto _ : state) {
        strategy.on_market_data(make_md(0, 15050 + tick, 15060 + tick));
        auto orders = strategy.generate_orders();
        benchmark::DoNotOptimize(orders.data());
        ++tick;
    }
}
BENCHMARK(BM_MomentumSignal);

BENCHMARK_MAIN();
