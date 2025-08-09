#include <benchmark/benchmark.h>
#include "risk/risk_manager.hpp"

using namespace trading;

static void BM_RiskCheckApproved(benchmark::State& state) {
    RiskLimits limits;
    limits.max_position_per_instrument = 100000;
    limits.max_total_position = 500000;
    limits.max_capital = 100'000'000.0;
    limits.max_order_size = 10000;
    limits.max_orders_per_second = 1000000;
    limits.max_price_deviation_pct = 50.0;
    RiskManager mgr(limits);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 10;
    req.timestamp = now_ns();

    for (auto _ : state) {
        auto result = mgr.check_order(req, 15000);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RiskCheckApproved);

static void BM_RiskCheckWithPosition(benchmark::State& state) {
    RiskLimits limits;
    limits.max_position_per_instrument = 100000;
    limits.max_total_position = 500000;
    limits.max_capital = 100'000'000.0;
    limits.max_order_size = 10000;
    limits.max_orders_per_second = 1000000;
    limits.max_price_deviation_pct = 50.0;
    RiskManager mgr(limits);

    // Pre-fill some positions
    mgr.position_tracker().on_fill(0, Side::Buy, 5000, 15000);
    mgr.position_tracker().update_mark_price(0, 15000);

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 10;
    req.timestamp = now_ns();

    for (auto _ : state) {
        auto result = mgr.check_order(req, 15000);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RiskCheckWithPosition);

static void BM_RiskCheckKillSwitch(benchmark::State& state) {
    RiskLimits limits;
    RiskManager mgr(limits);
    mgr.activate_kill_switch();

    OrderRequest req{};
    req.id = 1;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 10;

    for (auto _ : state) {
        auto result = mgr.check_order(req, 15000);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RiskCheckKillSwitch);

BENCHMARK_MAIN();
