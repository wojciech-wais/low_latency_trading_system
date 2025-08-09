#pragma once

#include "common/types.hpp"
#include "execution/exchange_simulator.hpp"
#include <vector>
#include <unordered_map>

namespace trading {

/// Routes orders to the best exchange based on price, latency, or round-robin.
/// Maintains order-to-exchange mapping for cancel routing.
class OrderRouter {
public:
    enum class RoutingStrategy {
        BestPrice,
        LowestLatency,
        RoundRobin
    };

    OrderRouter();

    void add_exchange(ExchangeSimulator* exchange);
    void set_routing_strategy(RoutingStrategy strategy) { strategy_ = strategy; }

    /// Route an order. Returns execution report.
    ExecutionReport route_order(const OrderRequest& request);

    /// Cancel an order (routes to correct exchange).
    ExecutionReport cancel_order(OrderId order_id);

    size_t exchange_count() const noexcept { return exchanges_.size(); }

private:
    ExchangeSimulator* select_exchange(const OrderRequest& request);

    std::vector<ExchangeSimulator*> exchanges_;
    std::unordered_map<OrderId, ExchangeId> order_exchange_map_;
    RoutingStrategy strategy_ = RoutingStrategy::RoundRobin;
    size_t round_robin_idx_ = 0;
};

} // namespace trading
