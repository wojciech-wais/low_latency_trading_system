#include "execution/order_router.hpp"
#include <algorithm>
#include <limits>

namespace trading {

OrderRouter::OrderRouter() = default;

void OrderRouter::add_exchange(ExchangeSimulator* exchange) {
    exchanges_.push_back(exchange);
}

ExecutionReport OrderRouter::route_order(const OrderRequest& request) {
    ExchangeSimulator* exchange = select_exchange(request);
    if (!exchange) {
        ExecutionReport report{};
        report.order_id = request.id;
        report.status = OrderStatus::Rejected;
        report.timestamp = now_ns();
        return report;
    }

    // Track which exchange got this order
    order_exchange_map_[request.id] = exchange->id();

    return exchange->submit_order(request);
}

ExecutionReport OrderRouter::cancel_order(OrderId order_id) {
    auto it = order_exchange_map_.find(order_id);
    if (it == order_exchange_map_.end()) {
        ExecutionReport report{};
        report.order_id = order_id;
        report.status = OrderStatus::Rejected;
        report.timestamp = now_ns();
        return report;
    }

    ExchangeId eid = it->second;
    for (auto* exchange : exchanges_) {
        if (exchange->id() == eid) {
            auto report = exchange->cancel_order(order_id);
            if (report.status == OrderStatus::Cancelled) {
                order_exchange_map_.erase(it);
            }
            return report;
        }
    }

    ExecutionReport report{};
    report.order_id = order_id;
    report.status = OrderStatus::Rejected;
    report.timestamp = now_ns();
    return report;
}

ExchangeSimulator* OrderRouter::select_exchange(const OrderRequest& request) {
    if (exchanges_.empty()) return nullptr;

    switch (strategy_) {
        case RoutingStrategy::LowestLatency: {
            ExchangeSimulator* best = nullptr;
            uint64_t min_latency = std::numeric_limits<uint64_t>::max();
            for (auto* exchange : exchanges_) {
                if (exchange->config().enabled && exchange->config().latency_ns < min_latency) {
                    min_latency = exchange->config().latency_ns;
                    best = exchange;
                }
            }
            return best ? best : exchanges_[0];
        }

        case RoutingStrategy::BestPrice:
            // For now, fall through to round-robin
            // Real implementation would check each exchange's book
            [[fallthrough]];

        case RoutingStrategy::RoundRobin:
        default:
            ExchangeSimulator* selected = exchanges_[round_robin_idx_ % exchanges_.size()];
            round_robin_idx_ = (round_robin_idx_ + 1) % exchanges_.size();
            return selected;
    }
}

} // namespace trading
