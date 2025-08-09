#pragma once

#include "common/types.hpp"
#include "common/config.hpp"
#include "containers/lock_free_queue.hpp"
#include "execution/exchange_simulator.hpp"
#include "execution/order_router.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

namespace trading {

/// Execution Engine: consumes OrderRequest from input queue,
/// routes to exchanges, produces ExecutionReport on output queue.
/// Includes order state machine and rate limiting.
class ExecutionEngine {
public:
    static constexpr size_t QUEUE_CAPACITY = 65536;

    using InputQueue = LockFreeRingBuffer<OrderRequest, QUEUE_CAPACITY>;
    using OutputQueue = LockFreeRingBuffer<ExecutionReport, QUEUE_CAPACITY>;

    ExecutionEngine(InputQueue& input, OutputQueue& output);

    /// Add an exchange simulator
    void add_exchange(const ExchangeConfig& config);

    /// Set order rate limit (orders per second)
    void set_rate_limit(uint32_t max_orders_per_sec) { max_orders_per_sec_ = max_orders_per_sec; }

    /// Set routing strategy
    void set_routing_strategy(OrderRouter::RoutingStrategy strategy);

    /// Process a single order (non-threaded, for testing)
    ExecutionReport process_order(const OrderRequest& request);

    /// Start engine thread pinned to core_id
    void start(int core_id);
    void stop();

    bool running() const noexcept { return running_.load(std::memory_order_relaxed); }
    uint64_t orders_processed() const noexcept { return orders_processed_; }
    uint64_t orders_throttled() const noexcept { return orders_throttled_; }

    /// Seed all exchange books
    void seed_books(Price mid_price, int levels, Quantity qty_per_level);

private:
    void run_loop(int core_id);
    bool check_rate_limit();

    InputQueue& input_;
    OutputQueue& output_;
    std::vector<std::unique_ptr<ExchangeSimulator>> exchanges_;
    OrderRouter router_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    uint32_t max_orders_per_sec_ = 10000;
    uint64_t orders_processed_ = 0;
    uint64_t orders_throttled_ = 0;

    // Rate limiter state
    Timestamp rate_window_start_ = 0;
    uint32_t orders_in_window_ = 0;
};

} // namespace trading
