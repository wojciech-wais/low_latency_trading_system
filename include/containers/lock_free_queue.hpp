#pragma once

#include "common/utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace trading {

/// Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
/// Capacity MUST be a power of 2 for efficient index masking.
/// Memory ordering: producer uses release on tail, consumer uses acquire on tail.
/// Each index owner loads its own index with relaxed ordering.
/// Buffer is heap-allocated once at construction (not on hot path).
template<typename T, size_t Capacity>
class LockFreeRingBuffer {
    static_assert(is_power_of_two(Capacity), "Capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    LockFreeRingBuffer()
        : buffer_(new T[Capacity]{}) {}

    /// Producer: attempt to push an item. Returns false if full.
    bool try_push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & MASK;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /// Consumer: attempt to pop an item. Returns false if empty.
    bool try_pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        item = buffer_[current_head];
        head_.store((current_head + 1) & MASK, std::memory_order_release);
        return true;
    }

    /// Approximate size (may be stale in multi-threaded context)
    size_t size() const noexcept {
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t head = head_.load(std::memory_order_acquire);
        return (tail - head) & MASK;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    bool full() const noexcept {
        const size_t next_tail = (tail_.load(std::memory_order_acquire) + 1) & MASK;
        return next_tail == head_.load(std::memory_order_acquire);
    }

    static constexpr size_t capacity() noexcept { return Capacity - 1; } // Usable capacity

private:
    static constexpr size_t MASK = Capacity - 1;

    // Separate cache lines for head and tail to prevent false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    // Heap-allocated buffer (single allocation at construction)
    std::unique_ptr<T[]> buffer_;
};

} // namespace trading
