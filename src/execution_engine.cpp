#include "execution/execution_engine.hpp"
#include "common/utils.hpp"

namespace trading {

ExecutionEngine::ExecutionEngine(InputQueue& input, OutputQueue& output)
    : input_(input), output_(output)
{}

void ExecutionEngine::add_exchange(const ExchangeConfig& config) {
    auto exchange = std::make_unique<ExchangeSimulator>(config);
    router_.add_exchange(exchange.get());
    exchanges_.push_back(std::move(exchange));
}

void ExecutionEngine::set_routing_strategy(OrderRouter::RoutingStrategy strategy) {
    router_.set_routing_strategy(strategy);
}

ExecutionReport ExecutionEngine::process_order(const OrderRequest& request) {
    if (!check_rate_limit()) {
        ++orders_throttled_;
        ExecutionReport report{};
        report.order_id = request.id;
        report.status = OrderStatus::Rejected;
        report.timestamp = now_ns();
        report.instrument = request.instrument;
        report.side = request.side;
        return report;
    }

    ++orders_processed_;
    return router_.route_order(request);
}

void ExecutionEngine::start(int core_id) {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&ExecutionEngine::run_loop, this, core_id);
}

void ExecutionEngine::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ExecutionEngine::run_loop(int core_id) {
    pin_thread_to_core(core_id);

    while (running_.load(std::memory_order_relaxed)) {
        OrderRequest request;
        if (input_.try_pop(request)) {
            ExecutionReport report = process_order(request);
            output_.try_push(report);
        }
    }

    // Drain remaining
    OrderRequest request;
    while (input_.try_pop(request)) {
        ExecutionReport report = process_order(request);
        output_.try_push(report);
    }
}

bool ExecutionEngine::check_rate_limit() {
    Timestamp now = now_ns();
    constexpr Timestamp ONE_SECOND_NS = 1'000'000'000ULL;

    if (now - rate_window_start_ >= ONE_SECOND_NS) {
        rate_window_start_ = now;
        orders_in_window_ = 0;
    }

    if (orders_in_window_ >= max_orders_per_sec_) {
        return false;
    }

    ++orders_in_window_;
    return true;
}

void ExecutionEngine::seed_books(Price mid_price, int levels, Quantity qty_per_level) {
    for (auto& exchange : exchanges_) {
        exchange->seed_book(mid_price, levels, qty_per_level);
    }
}

} // namespace trading
