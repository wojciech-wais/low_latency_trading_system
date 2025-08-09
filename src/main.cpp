#include "common/types.hpp"
#include "common/config.hpp"
#include "common/logger.hpp"
#include "common/utils.hpp"
#include "containers/lock_free_queue.hpp"
#include "market_data/feed_simulator.hpp"
#include "market_data/market_data_handler.hpp"
#include "order_book/order_book.hpp"
#include "strategy/market_maker.hpp"
#include "strategy/pairs_trading.hpp"
#include "strategy/momentum.hpp"
#include "execution/execution_engine.hpp"
#include "risk/risk_manager.hpp"
#include "monitoring/metrics_collector.hpp"

#include <csignal>
#include <cstdio>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

namespace {
    std::atomic<bool> g_running{true};

    void signal_handler(int) {
        g_running.store(false, std::memory_order_relaxed);
    }
}

int main(int argc, char* argv[]) {
    using namespace trading;

    // --- Signal handling ---
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // --- Load config ---
    SystemConfig config;
    if (argc > 1) {
        config = load_config(argv[1]);
        printf("Loaded config from: %s\n", argv[1]);
    } else {
        config = default_config();
        printf("Using default configuration\n");
    }

    printf("\n");
    printf("=== Ultra-Low Latency HFT Trading Simulator ===\n");
    printf("    Starting up...\n\n");

    // --- Start logger ---
    Logger::instance().start();
    LOG_INFO("System starting up");

    // --- Initialize components ---

    // Queues (heap-allocated to avoid stack issues)
    auto md_queue_ptr = std::make_unique<MarketDataHandler::OutputQueue>();
    auto order_queue_ptr = std::make_unique<ExecutionEngine::InputQueue>();
    auto exec_report_queue_ptr = std::make_unique<ExecutionEngine::OutputQueue>();
    auto& md_queue = *md_queue_ptr;
    auto& order_queue = *order_queue_ptr;
    auto& exec_report_queue = *exec_report_queue_ptr;

    // Feed simulator
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 15000, config.volatility, 2, 100);
    feed.add_instrument(1, "GOOG", 28000, config.volatility * 1.2, 3, 50);
    printf("  Feed simulator:    2 instruments (AAPL, GOOG)\n");

    // Market data handler
    MarketDataHandler md_handler(md_queue);
    printf("  Market data handler: ready\n");

    // Order books
    OrderBook book_aapl(0);
    OrderBook book_goog(1);
    printf("  Order books:       AAPL, GOOG\n");

    // Strategies
    MarketMakerStrategy::Params mm_params;
    mm_params.base_spread_bps = config.market_maker_spread_bps;
    mm_params.max_inventory = config.market_maker_max_inventory;
    mm_params.order_size = 10;
    mm_params.instrument = 0;
    MarketMakerStrategy market_maker(mm_params);

    PairsTradingStrategy::Params pairs_params;
    pairs_params.instrument_a = 0;
    pairs_params.instrument_b = 1;
    pairs_params.lookback_window = static_cast<size_t>(config.pairs_lookback_window);
    pairs_params.entry_z_threshold = config.pairs_entry_z;
    pairs_params.exit_z_threshold = config.pairs_exit_z;
    PairsTradingStrategy pairs_strategy(pairs_params);

    MomentumStrategy::Params mom_params;
    mom_params.instrument = 0;
    mom_params.fast_window = config.momentum_fast_window;
    mom_params.slow_window = config.momentum_slow_window;
    mom_params.breakout_threshold_bps = config.momentum_breakout_bps;
    MomentumStrategy momentum_strategy(mom_params);

    printf("  Strategies:        MarketMaker, PairsTrading, Momentum\n");

    // Risk manager
    RiskManager risk_mgr(config.risk_limits);
    printf("  Risk manager:      ready\n");

    // Execution engine
    ExecutionEngine exec_engine(order_queue, exec_report_queue);
    for (size_t i = 0; i < config.num_exchanges; ++i) {
        exec_engine.add_exchange(config.exchanges[i]);
    }
    exec_engine.seed_books(15000, 10, 1000);
    printf("  Execution engine:  %zu exchanges\n", config.num_exchanges);

    // Metrics
    MetricsCollector metrics;

    printf("\n  Starting simulation (duration: %lu ms, Ctrl+C to stop)...\n\n",
           static_cast<unsigned long>(config.simulation_duration_ms));

    // --- Main hot loop ---
    auto start_time = std::chrono::steady_clock::now();
    Timestamp sim_start_ns = now_ns();
    uint64_t sim_duration_ns = config.simulation_duration_ms * 1'000'000ULL;
    uint64_t iteration = 0;

    // Start execution engine thread
    exec_engine.start(config.execution_core);

    while (g_running.load(std::memory_order_relaxed)) {
        Timestamp loop_start = now_ns();

        // Check simulation duration
        if (loop_start - sim_start_ns > sim_duration_ns) {
            break;
        }

        // 1. Generate market data
        Timestamp t0 = now_ns();
        std::string_view fix_msg = feed.next_message();
        if (!fix_msg.empty()) {
            md_handler.process_message(fix_msg);
        }
        Timestamp t1 = now_ns();
        metrics.market_data_latency().record(t1 - t0);
        metrics.record_market_data_msg();

        // 2. Consume market data → update order book
        MarketDataMessage md;
        if (md_queue.try_pop(md)) {
            Timestamp t2 = now_ns();

            if (md.instrument == 0 && md.bid_price > 0 && md.ask_price > 0) {
                // Update AAPL book BBO tracking
            }
            if (md.instrument == 1 && md.bid_price > 0 && md.ask_price > 0) {
                // Update GOOG book BBO tracking
            }
            metrics.record_order_book_update();

            Timestamp t3 = now_ns();
            metrics.order_book_latency().record(t3 - t2);

            // 3. Feed to strategies
            Timestamp t4 = now_ns();
            market_maker.on_market_data(md);
            pairs_strategy.on_market_data(md);
            momentum_strategy.on_market_data(md);

            // Generate orders from market maker
            auto mm_orders = market_maker.generate_orders();
            for (const auto& order_req : mm_orders) {
                // 4. Risk check
                Timestamp t5 = now_ns();
                Price market_price = (md.bid_price + md.ask_price) / 2;
                auto risk_result = risk_mgr.check_order(order_req, market_price);
                Timestamp t6 = now_ns();
                metrics.risk_check_latency().record(t6 - t5);

                if (risk_result == RiskCheckResult::Approved) {
                    // 5. Send to execution
                    order_queue.try_push(order_req);
                    metrics.record_order_sent();
                }
            }

            // Generate from pairs strategy
            auto pairs_orders = pairs_strategy.generate_orders();
            for (const auto& order_req : pairs_orders) {
                Price market_price = (md.bid_price + md.ask_price) / 2;
                auto risk_result = risk_mgr.check_order(order_req, market_price);
                if (risk_result == RiskCheckResult::Approved) {
                    order_queue.try_push(order_req);
                    metrics.record_order_sent();
                }
            }

            // Generate from momentum strategy
            auto mom_orders = momentum_strategy.generate_orders();
            for (const auto& order_req : mom_orders) {
                Price market_price = (md.bid_price + md.ask_price) / 2;
                auto risk_result = risk_mgr.check_order(order_req, market_price);
                if (risk_result == RiskCheckResult::Approved) {
                    order_queue.try_push(order_req);
                    metrics.record_order_sent();
                }
            }

            Timestamp t7 = now_ns();
            metrics.strategy_latency().record(t7 - t4);

            // Tick-to-trade
            Timestamp tick_to_trade = t7 - t0;
            metrics.tick_to_trade_latency().record(tick_to_trade);
            metrics.tick_to_trade_histogram().record(tick_to_trade);
        }

        // 6. Process execution reports
        ExecutionReport report;
        while (exec_report_queue.try_pop(report)) {
            market_maker.on_execution_report(report);
            pairs_strategy.on_execution_report(report);
            momentum_strategy.on_execution_report(report);

            if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
                risk_mgr.position_tracker().on_fill(
                    report.instrument, report.side, report.filled_quantity, report.price);
                metrics.record_fill();
            }

            // Update mark prices
            if (report.price > 0) {
                risk_mgr.position_tracker().update_mark_price(report.instrument, report.price);
            }

            // Drawdown check
            risk_mgr.on_pnl_update(risk_mgr.position_tracker().total_pnl());
        }

        ++iteration;
    }

    // --- Shutdown ---
    exec_engine.stop();
    Logger::instance().stop();

    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();

    // --- Report ---
    metrics.print_summary(elapsed);

    printf("\n--- Position Summary ---\n");
    printf("  AAPL position: %ld\n", static_cast<long>(risk_mgr.position_tracker().position(0)));
    printf("  GOOG position: %ld\n", static_cast<long>(risk_mgr.position_tracker().position(1)));
    printf("  Realized P&L:  $%.2f\n", risk_mgr.position_tracker().realized_pnl());
    printf("  Total P&L:     $%.2f\n", risk_mgr.position_tracker().total_pnl());
    printf("\n  Iterations: %lu\n", static_cast<unsigned long>(iteration));
    printf("  Risk checks: %lu (rejected: %lu)\n",
           static_cast<unsigned long>(risk_mgr.checks_performed()),
           static_cast<unsigned long>(risk_mgr.checks_rejected()));

    if (risk_mgr.kill_switch_active()) {
        printf("  WARNING: Kill switch was activated!\n");
    }

    printf("\nSimulation complete.\n");
    return 0;
}
