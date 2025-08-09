#pragma once

#include "common/types.hpp"
#include "containers/lock_free_queue.hpp"
#include "market_data/fix_parser.hpp"
#include <atomic>
#include <thread>
#include <string_view>
#include <functional>

namespace trading {

/// Market Data Handler: parses FIX messages and pushes MarketDataMessage
/// to a lock-free queue for downstream consumption.
class MarketDataHandler {
public:
    static constexpr size_t QUEUE_CAPACITY = 65536;

    using OutputQueue = LockFreeRingBuffer<MarketDataMessage, QUEUE_CAPACITY>;

    explicit MarketDataHandler(OutputQueue& output_queue);

    /// Process a single FIX message. Returns true if parsed and enqueued.
    bool process_message(std::string_view raw_message) noexcept;

    /// Start handler thread pinned to core_id.
    /// feed_callback is called repeatedly to get messages.
    void start(int core_id, std::function<std::string_view()> feed_callback);
    void stop();

    bool running() const noexcept { return running_.load(std::memory_order_relaxed); }

    uint64_t messages_processed() const noexcept { return messages_processed_; }
    uint64_t messages_dropped() const noexcept { return messages_dropped_; }

    /// Map instrument symbol to ID
    static InstrumentId symbol_to_id(std::string_view symbol) noexcept;

private:
    void run_loop(int core_id, std::function<std::string_view()> feed_callback);

    OutputQueue& output_queue_;
    FixParser parser_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    uint64_t messages_processed_ = 0;
    uint64_t messages_dropped_ = 0;
};

} // namespace trading
