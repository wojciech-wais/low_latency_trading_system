#include <benchmark/benchmark.h>
#include "common/types.hpp"
#include "containers/lock_free_queue.hpp"
#include "market_data/feed_simulator.hpp"
#include "market_data/market_data_handler.hpp"
#include "order_book/order_book.hpp"
#include "strategy/market_maker.hpp"
#include "risk/risk_manager.hpp"
#include "execution/execution_engine.hpp"
#include "monitoring/latency_tracker.hpp"
#include "monitoring/histogram.hpp"

using namespace trading;

static void BM_TickToTrade(benchmark::State& state) {
    MarketDataHandler::OutputQueue md_queue;
    ExecutionEngine::InputQueue order_queue;
    ExecutionEngine::OutputQueue exec_queue;

    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);

    MarketDataHandler md_handler(md_queue);

    MarketMakerStrategy::Params mm_params;
    mm_params.instrument = 0;
    mm_params.order_size = 10;
    MarketMakerStrategy mm(mm_params);

    RiskLimits limits;
    limits.max_position_per_instrument = 100000;
    limits.max_total_position = 500000;
    limits.max_order_size = 10000;
    limits.max_orders_per_second = 1000000;
    limits.max_capital = 100'000'000.0;
    limits.max_price_deviation_pct = 50.0;
    RiskManager risk_mgr(limits);

    for (auto _ : state) {
        Timestamp t_start = now_ns();

        // 1. Generate and parse market data
        auto fix_msg = feed.next_message();
        md_handler.process_message(fix_msg);

        // 2. Consume
        MarketDataMessage md;
        if (md_queue.try_pop(md)) {
            // 3. Strategy
            mm.on_market_data(md);
            auto orders = mm.generate_orders();

            // 4. Risk check + enqueue
            for (const auto& req : orders) {
                Price market_price = (md.bid_price + md.ask_price) / 2;
                risk_mgr.check_order(req, market_price);
                order_queue.try_push(req);
            }
        }

        Timestamp t_end = now_ns();
        benchmark::DoNotOptimize(t_end - t_start);

        // Drain order queue
        OrderRequest drain;
        while (order_queue.try_pop(drain)) {}
    }
}
BENCHMARK(BM_TickToTrade)->Iterations(1000000);

BENCHMARK_MAIN();
