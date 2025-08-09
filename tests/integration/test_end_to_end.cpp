#include <gtest/gtest.h>
#include "common/types.hpp"
#include "common/config.hpp"
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

using namespace trading;

TEST(EndToEndTest, FullPipeline) {
    // Setup queues
    MarketDataHandler::OutputQueue md_queue;
    ExecutionEngine::InputQueue order_queue;
    ExecutionEngine::OutputQueue exec_queue;

    // Feed simulator
    FeedSimulator feed;
    feed.add_instrument(0, "AAPL", 150.00, 0.001, 0.02, 100);
    feed.add_instrument(1, "GOOG", 280.00, 0.001, 0.03, 50);

    // Market data handler
    MarketDataHandler md_handler(md_queue);

    // Strategies
    MarketMakerStrategy::Params mm_params;
    mm_params.instrument = 0;
    mm_params.base_spread_bps = 10.0;
    mm_params.max_inventory = 100;
    mm_params.order_size = 10;
    MarketMakerStrategy market_maker(mm_params);

    PairsTradingStrategy::Params pairs_params;
    pairs_params.instrument_a = 0;
    pairs_params.instrument_b = 1;
    PairsTradingStrategy pairs_strategy(pairs_params);

    MomentumStrategy::Params mom_params;
    mom_params.instrument = 0;
    MomentumStrategy momentum(mom_params);

    // Risk manager
    RiskLimits limits;
    limits.max_position_per_instrument = 10000;
    limits.max_total_position = 50000;
    limits.max_order_size = 1000;
    limits.max_orders_per_second = 100000;
    limits.max_capital = 10'000'000.0;
    limits.max_price_deviation_pct = 50.0;
    RiskManager risk_mgr(limits);

    // Execution engine
    ExecutionEngine exec_engine(order_queue, exec_queue);
    exec_engine.add_exchange({0, "TEST_1", 100, 1.0, true});
    exec_engine.add_exchange({1, "TEST_2", 200, 1.0, true});
    exec_engine.seed_books(15000, 10, 1000);
    exec_engine.start(0);

    // Process 10K messages
    uint64_t orders_sent = 0;
    uint64_t fills = 0;

    for (int i = 0; i < 10000; ++i) {
        // Generate and process market data
        auto fix_msg = feed.next_message();
        md_handler.process_message(fix_msg);

        MarketDataMessage md;
        if (md_queue.try_pop(md)) {
            market_maker.on_market_data(md);
            pairs_strategy.on_market_data(md);
            momentum.on_market_data(md);

            auto mm_orders = market_maker.generate_orders();
            for (const auto& req : mm_orders) {
                Price market_price = (md.bid_price + md.ask_price) / 2;
                auto result = risk_mgr.check_order(req, market_price);
                if (result == RiskCheckResult::Approved) {
                    order_queue.try_push(req);
                    ++orders_sent;
                }
            }
        }

        // Process execution reports
        ExecutionReport report;
        while (exec_queue.try_pop(report)) {
            market_maker.on_execution_report(report);
            if (report.status == OrderStatus::Filled || report.status == OrderStatus::PartiallyFilled) {
                risk_mgr.position_tracker().on_fill(
                    report.instrument, report.side, report.filled_quantity, report.price);
                ++fills;
            }
        }
    }

    exec_engine.stop();

    // Verify: messages were processed, orders generated, some fills occurred
    EXPECT_GT(md_handler.messages_processed(), 0u);
    EXPECT_GT(orders_sent, 0u);
    EXPECT_GT(risk_mgr.checks_performed(), 0u);

    printf("  End-to-end: %lu MD msgs, %lu orders sent, %lu fills\n",
           static_cast<unsigned long>(md_handler.messages_processed()),
           static_cast<unsigned long>(orders_sent),
           static_cast<unsigned long>(fills));
}

TEST(EndToEndTest, RiskCheckIntegration) {
    RiskLimits limits;
    limits.max_position_per_instrument = 50;
    limits.max_order_size = 100;
    limits.max_orders_per_second = 100000;
    limits.max_capital = 10'000'000.0;
    limits.max_price_deviation_pct = 50.0;
    RiskManager risk_mgr(limits);

    MarketMakerStrategy::Params params;
    params.instrument = 0;
    params.max_inventory = 100;
    params.order_size = 10;
    MarketMakerStrategy mm(params);

    // Feed data and generate orders
    MarketDataMessage md{};
    md.instrument = 0;
    md.bid_price = 15000;
    md.ask_price = 15010;
    md.bid_quantity = 100;
    md.ask_quantity = 100;
    md.timestamp = now_ns();
    md.msg_type = 'W';
    mm.on_market_data(md);

    auto orders = mm.generate_orders();
    ASSERT_FALSE(orders.empty());

    // All orders should pass risk checks initially
    for (const auto& req : orders) {
        auto result = risk_mgr.check_order(req, 15005);
        EXPECT_EQ(result, RiskCheckResult::Approved);
    }

    // Fill up position to trigger breach
    for (int i = 0; i < 5; ++i) {
        risk_mgr.position_tracker().on_fill(0, Side::Buy, 10, 15000);
    }

    // Now position is 50, at limit — next buy should be rejected
    OrderRequest req{};
    req.id = 999;
    req.instrument = 0;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = 15000;
    req.quantity = 10;
    req.timestamp = now_ns();

    auto result = risk_mgr.check_order(req, 15000);
    EXPECT_EQ(result, RiskCheckResult::PositionLimitBreached);
}
