#include <benchmark/benchmark.h>
#include "execution/execution_engine.hpp"

using namespace trading;

static void BM_ExecutionEngineProcess(benchmark::State& state) {
    ExecutionEngine::InputQueue input;
    ExecutionEngine::OutputQueue output;
    ExecutionEngine engine(input, output);
    engine.add_exchange({0, "TEST", 100, 1.0, true});
    engine.seed_books(15000, 10, 10000);
    engine.set_rate_limit(1000000); // High limit for benchmarking

    OrderId id = 1;
    for (auto _ : state) {
        OrderRequest req{};
        req.id = id++;
        req.instrument = 0;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.price = 15000;
        req.quantity = 10;
        req.timestamp = now_ns();

        auto report = engine.process_order(req);
        benchmark::DoNotOptimize(report);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ExecutionEngineProcess);

BENCHMARK_MAIN();
