#include "execution/exchange_simulator.hpp"
#include <chrono>

namespace trading {

ExchangeSimulator::ExchangeSimulator(const ExchangeConfig& config)
    : config_(config)
    , book_(0)
    , rng_(config.id * 1000 + 42)
{}

ExecutionReport ExchangeSimulator::submit_order(const OrderRequest& request) {
    ++orders_processed_;

    ExecutionReport report{};
    report.order_id = request.id;
    report.exec_id = next_exec_id_++;
    report.instrument = request.instrument;
    report.side = request.side;
    report.exchange = config_.id;
    report.timestamp = now_ns() + config_.latency_ns; // Simulated latency

    // Check fill probability
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rng_) > config_.fill_probability) {
        report.status = OrderStatus::Rejected;
        report.price = request.price;
        report.quantity = request.quantity;
        report.filled_quantity = 0;
        report.leaves_quantity = request.quantity;
        ++rejects_;
        return report;
    }

    // Submit to internal order book
    auto trades = book_.add_order(request.id, request.side, request.type,
                                   request.price, request.quantity, report.timestamp);

    if (!trades.empty()) {
        // Got fills
        Quantity total_filled = 0;
        Price last_fill_price = 0;
        for (const auto& trade : trades) {
            total_filled += trade.quantity;
            last_fill_price = trade.price;
        }

        report.filled_quantity = total_filled;
        report.leaves_quantity = request.quantity - total_filled;
        report.price = last_fill_price;

        if (report.leaves_quantity == 0) {
            report.status = OrderStatus::Filled;
        } else {
            report.status = OrderStatus::PartiallyFilled;
        }
        ++fills_;
    } else {
        // Resting on book (or IOC/FOK cancelled)
        if (request.type == OrderType::IOC || request.type == OrderType::Market) {
            report.status = OrderStatus::Cancelled;
            report.filled_quantity = 0;
            report.leaves_quantity = request.quantity;
        } else {
            report.status = OrderStatus::New;
            report.price = request.price;
            report.quantity = request.quantity;
            report.filled_quantity = 0;
            report.leaves_quantity = request.quantity;
        }
    }

    return report;
}

ExecutionReport ExchangeSimulator::cancel_order(OrderId order_id) {
    ExecutionReport report{};
    report.order_id = order_id;
    report.exec_id = next_exec_id_++;
    report.exchange = config_.id;
    report.timestamp = now_ns() + config_.latency_ns;

    if (book_.cancel_order(order_id)) {
        report.status = OrderStatus::Cancelled;
    } else {
        report.status = OrderStatus::Rejected;
    }

    return report;
}

void ExchangeSimulator::seed_book(Price mid_price, int levels, Quantity qty_per_level) {
    OrderId oid = 900000000;
    for (int i = 1; i <= levels; ++i) {
        // Bids below mid
        book_.add_order(oid++, Side::Buy, OrderType::Limit,
                        mid_price - i, qty_per_level, now_ns());
        // Asks above mid
        book_.add_order(oid++, Side::Sell, OrderType::Limit,
                        mid_price + i, qty_per_level, now_ns());
    }
}

void ExchangeSimulator::update_book(const MarketDataMessage& md) {
    // Could update book state from external market data
    // For now, the simulator's internal book is self-contained
}

} // namespace trading
