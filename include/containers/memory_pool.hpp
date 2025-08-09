#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace trading {

/// Fixed-size memory pool with O(1) allocate/deallocate.
/// Single-threaded only — no atomics overhead.
/// Uses an index-based intrusive free list over a contiguous heap-allocated array.
/// Storage is allocated once at construction (not on hot path).
template<typename T, size_t PoolSize>
class MemoryPool {
    static_assert(sizeof(T) >= sizeof(uint32_t), "T must be at least 4 bytes for free list index");

public:
    using StorageType = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    MemoryPool() : storage_(std::make_unique<StorageType[]>(PoolSize)) {
        // Initialize free list: each slot points to the next
        for (uint32_t i = 0; i < PoolSize - 1; ++i) {
            *reinterpret_cast<uint32_t*>(&storage_[i]) = i + 1;
        }
        *reinterpret_cast<uint32_t*>(&storage_[PoolSize - 1]) = INVALID;
        free_head_ = 0;
        allocated_count_ = 0;
    }

    /// Allocate one object. Returns nullptr if pool is exhausted.
    T* allocate() noexcept {
        if (free_head_ == INVALID) {
            return nullptr;
        }
        uint32_t idx = free_head_;
        free_head_ = *reinterpret_cast<uint32_t*>(&storage_[idx]);
        ++allocated_count_;
        return reinterpret_cast<T*>(&storage_[idx]);
    }

    /// Deallocate a previously allocated object.
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        auto* raw = reinterpret_cast<StorageType*>(ptr);
        size_t idx = static_cast<size_t>(raw - storage_.get());
        *reinterpret_cast<uint32_t*>(&storage_[idx]) = free_head_;
        free_head_ = static_cast<uint32_t>(idx);
        --allocated_count_;
    }

    /// Check if a pointer belongs to this pool.
    bool owns(const T* ptr) const noexcept {
        auto* raw = reinterpret_cast<const StorageType*>(ptr);
        return raw >= storage_.get() && raw < storage_.get() + PoolSize;
    }

    size_t allocated() const noexcept { return allocated_count_; }
    size_t available() const noexcept { return PoolSize - allocated_count_; }
    static constexpr size_t pool_size() noexcept { return PoolSize; }

private:
    static constexpr uint32_t INVALID = 0xFFFFFFFF;

    std::unique_ptr<StorageType[]> storage_;
    uint32_t free_head_ = 0;
    size_t allocated_count_ = 0;
};

} // namespace trading
