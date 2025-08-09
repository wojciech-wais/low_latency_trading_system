#include "market_data/market_data_handler.hpp"
#include "common/utils.hpp"

namespace trading {

MarketDataHandler::MarketDataHandler(OutputQueue& output_queue)
    : output_queue_(output_queue)
{}

bool MarketDataHandler::process_message(std::string_view raw_message) noexcept {
    if (!parser_.parse(raw_message)) {
        return false;
    }

    auto msg_type = parser_.msg_type();

    MarketDataMessage md{};
    md.timestamp = now_ns();

    if (msg_type == "W") {
        // Market data snapshot
        md.msg_type = 'W';
        md.instrument = symbol_to_id(parser_.get_symbol());
        md.bid_price = parser_.get_bid_price();
        md.ask_price = parser_.get_ask_price();
        md.bid_quantity = parser_.get_bid_size();
        md.ask_quantity = parser_.get_ask_size();
        md.last_price = parser_.get_price();
        md.last_quantity = parser_.get_quantity();
    } else if (msg_type == "8") {
        // Execution report — could be used for fill notifications
        md.msg_type = '8';
        md.instrument = symbol_to_id(parser_.get_symbol());
        md.last_price = parser_.get_price();
        md.last_quantity = parser_.get_quantity();
    } else if (msg_type == "D") {
        // New order single
        md.msg_type = 'D';
        md.instrument = symbol_to_id(parser_.get_symbol());
        md.last_price = parser_.get_price();
        md.last_quantity = parser_.get_quantity();
    } else {
        return false; // Unsupported message type
    }

    if (output_queue_.try_push(md)) {
        ++messages_processed_;
        return true;
    } else {
        ++messages_dropped_;
        return false;
    }
}

void MarketDataHandler::start(int core_id, std::function<std::string_view()> feed_callback) {
    if (running_.exchange(true)) return;
    thread_ = std::thread(&MarketDataHandler::run_loop, this, core_id, std::move(feed_callback));
}

void MarketDataHandler::stop() {
    running_.store(false, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void MarketDataHandler::run_loop(int core_id, std::function<std::string_view()> feed_callback) {
    pin_thread_to_core(core_id);

    while (running_.load(std::memory_order_relaxed)) {
        std::string_view msg = feed_callback();
        if (!msg.empty()) {
            process_message(msg);
        }
    }
}

InstrumentId MarketDataHandler::symbol_to_id(std::string_view symbol) noexcept {
    // Simple hash-based mapping for known instruments
    if (symbol == "AAPL") return 0;
    if (symbol == "GOOG") return 1;
    if (symbol == "MSFT") return 2;
    if (symbol == "AMZN") return 3;
    if (symbol == "TSLA") return 4;

    // Generic hash for unknown symbols
    uint32_t hash = 0;
    for (char c : symbol) {
        hash = hash * 31 + static_cast<uint32_t>(c);
    }
    return hash % MAX_INSTRUMENTS;
}

} // namespace trading
