---
title: "Technical Deep Dive"
subtitle: "Lock-free data structures, risk management, and memory pool design"
---

## Lock-Free SPSC Ring Buffer

The core communication primitive is a Single-Producer Single-Consumer ring buffer. It provides wait-free push/pop operations using only `acquire`/`release` memory ordering — never the heavier `seq_cst`.

```cpp
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

private:
    static constexpr size_t MASK = Capacity - 1;

    // Separate cache lines to prevent false sharing
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

    // Heap-allocated buffer (single allocation at construction)
    std::unique_ptr<T[]> buffer_;
};
```

### Key Design Points

- **Power-of-2 capacity**: Enables bitwise masking (`index & MASK`) instead of modulo for index wrapping — a single AND instruction vs. expensive division
- **Relaxed loads for own index**: The producer only writes `tail_` and the consumer only writes `head_`. Each thread loads its own index with `memory_order_relaxed` since there's no contention
- **Acquire/release pairing**: The producer does a `release` store on `tail_` after writing data, guaranteeing the consumer sees the data when it does an `acquire` load on `tail_`
- **Cache-line separation**: `alignas(64)` on `head_` and `tail_` ensures they're on different cache lines, preventing false sharing between producer and consumer cores
- **Heap-allocated buffer**: Uses `std::unique_ptr<T[]>` instead of `std::array`. A 65536-element array would be ~5MB on the stack, causing stack overflow

## Risk Manager: 21ns Pre-Trade Checks

The risk manager runs 6 sequential checks on every order, completing in approximately 21 nanoseconds. The key techniques:

```cpp
class RiskManager {
public:
    /// Check an order against all risk limits. O(1), <100ns target.
    __attribute__((hot))
    RiskCheckResult check_order(
        const OrderRequest& request,
        Price current_market_price
    ) noexcept;

private:
    alignas(64) std::atomic<bool> kill_switch_{false};

    // Precomputed reciprocal to avoid division on hot path
    double price_deviation_threshold_;

    // Flat array position tracking (O(1) by instrument ID)
    PositionTracker positions_;
};
```

### The Six Checks (~21ns total)

| # | Check | Implementation | Cost |
|---|-------|---------------|------|
| 1 | Kill switch | Atomic load | ~1 ns |
| 2 | Order size | Single comparison | ~1 ns |
| 3 | Position limit | Array lookup + abs | ~3 ns |
| 4 | Capital limit | Multiplication | ~3 ns |
| 5 | Order rate | Counter check | ~2 ns |
| 6 | Fat finger | Multiply, not divide | ~3 ns |

### Why It's Fast

- **`[[unlikely]]` on all failure branches**: The compiler lays out the approved path as straight-line code with no branches taken. Rejection paths are moved out-of-line
- **No virtual dispatch**: Concrete class, no inheritance, direct function calls
- **Flat array for positions**: `PositionTracker` uses a fixed-size array indexed by `InstrumentId` — O(1) lookup, no hash computation
- **Multiplication over division**: Fat finger check uses `abs(order_price - market_price) * reciprocal > market_price` instead of dividing, since multiplication is ~3x faster than division on modern CPUs
- **`__attribute__((hot))`**: Hints to the compiler to optimize this function aggressively and place it in the hot section of the binary

## Memory Pool: Zero-Allocation Order Management

Orders are allocated from a pre-allocated memory pool to avoid `new`/`delete` on the hot path:

```cpp
template<typename T, size_t PoolSize>
class MemoryPool {
public:
    MemoryPool() : storage_(std::make_unique<StorageType[]>(PoolSize)) {
        // Initialize free list: each slot points to the next
        for (uint32_t i = 0; i < PoolSize - 1; ++i) {
            *reinterpret_cast<uint32_t*>(&storage_[i]) = i + 1;
        }
        *reinterpret_cast<uint32_t*>(&storage_[PoolSize - 1]) = INVALID;
    }

    /// O(1) allocate: pop from free list head
    T* allocate() noexcept {
        if (free_head_ == INVALID) return nullptr;
        uint32_t idx = free_head_;
        free_head_ = *reinterpret_cast<uint32_t*>(&storage_[idx]);
        ++allocated_count_;
        return reinterpret_cast<T*>(&storage_[idx]);
    }

    /// O(1) deallocate: push to free list head
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        auto* raw = reinterpret_cast<StorageType*>(ptr);
        size_t idx = static_cast<size_t>(raw - storage_.get());
        *reinterpret_cast<uint32_t*>(&storage_[idx]) = free_head_;
        free_head_ = static_cast<uint32_t>(idx);
        --allocated_count_;
    }

private:
    static constexpr uint32_t INVALID = 0xFFFFFFFF;
    std::unique_ptr<StorageType[]> storage_;
    uint32_t free_head_ = 0;
    size_t allocated_count_ = 0;
};
```

### Design Insights

- **Intrusive free list**: The free list index is stored *inside* the unused memory slots themselves. When a slot is free, its first 4 bytes contain the index of the next free slot. This means the free list requires zero additional memory
- **Single heap allocation**: `std::make_unique<StorageType[]>(PoolSize)` allocates all storage at construction. After that, `allocate()` and `deallocate()` never touch the heap
- **No atomics**: The pool is single-threaded by design (each component owns its pool), so there's no atomic overhead
- **Contiguous memory**: All objects are in a contiguous array, maximizing cache locality during iteration

## Strategy Engine: Pre-Allocated Signal Output

Strategies return their order signals via `std::span` over pre-allocated member arrays. This avoids allocating `std::vector` on every tick:

```cpp
class StrategyInterface {
public:
    /// Generate orders based on market data update.
    /// Returns a span over internally-owned buffer (no allocation).
    virtual std::span<const OrderRequest> on_market_data(
        const MarketDataMessage& msg,
        const OrderBook& book
    ) = 0;
};
```

Each strategy implementation has a `std::array<OrderRequest, MAX_ORDERS>` member and a count, returning a `std::span` view over the filled portion. The caller never allocates, and the span is valid until the next call.

## FIX Parser: Zero-Copy Field Access

The FIX protocol parser achieves ~700ns per message using zero-copy techniques:

- **`string_view` into raw buffer**: Field values are views into the original message bytes — no string copies
- **Flat array tag lookup**: A `std::array<StringView, MAX_TAG>` indexed by FIX tag number provides O(1) field access after parsing
- **Single-pass parsing**: One scan through the message populates all tag-value pairs

## Lessons Learned

<div class="info-box green">
<strong>Stack overflow with large templates:</strong> <code>std::array&lt;T, 65536&gt;</code> in class templates caused stack overflow when objects were stack-allocated (~5MB per instance). Fixed by using <code>std::unique_ptr&lt;T[]&gt;</code> with heap allocation at construction. This single allocation at startup doesn't affect hot-path performance.
</div>

<div class="info-box orange">
<strong>Default-initialized enums:</strong> Default-initialized enum fields (value 0) can accidentally match routing conditions. The order router had a bug where default-constructed orders would match the first routing rule. Fixed by explicitly checking for initialized state before routing.
</div>

<div class="info-box">
<strong>Memory ordering matters:</strong> Initial implementation used <code>memory_order_seq_cst</code> everywhere "to be safe." Switching to the correct <code>acquire</code>/<code>release</code> pairs on the SPSC queue improved throughput measurably — <code>seq_cst</code> emits unnecessary fence instructions on x86.
</div>
