#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>

namespace trading {

/// Fixed-size circular buffer (ring buffer) for rolling windows.
/// push_back() overwrites the oldest element when full.
/// Supports random access via operator[], back(), and iterators.
/// Buffer is heap-allocated once at construction (not on hot path).
template<typename T, size_t Capacity>
class CircularBuffer {
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    CircularBuffer() : buffer_(new T[Capacity]{}) {}

    void push_back(const T& value) noexcept {
        buffer_[write_pos_] = value;
        write_pos_ = (write_pos_ + 1) % Capacity;
        if (count_ < Capacity) {
            ++count_;
        }
    }

    /// Access element by logical index (0 = oldest, size()-1 = newest).
    const T& operator[](size_t idx) const noexcept {
        size_t start = (count_ < Capacity) ? 0 : write_pos_;
        return buffer_[(start + idx) % Capacity];
    }

    T& operator[](size_t idx) noexcept {
        size_t start = (count_ < Capacity) ? 0 : write_pos_;
        return buffer_[(start + idx) % Capacity];
    }

    const T& back() const noexcept {
        return buffer_[(write_pos_ + Capacity - 1) % Capacity];
    }

    T& back() noexcept {
        return buffer_[(write_pos_ + Capacity - 1) % Capacity];
    }

    const T& front() const noexcept {
        size_t start = (count_ < Capacity) ? 0 : write_pos_;
        return buffer_[start];
    }

    size_t size() const noexcept { return count_; }
    bool empty() const noexcept { return count_ == 0; }
    bool full() const noexcept { return count_ == Capacity; }
    static constexpr size_t capacity() noexcept { return Capacity; }

    void clear() noexcept {
        write_pos_ = 0;
        count_ = 0;
    }

    // Iterator support
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;
        using iterator_category = std::random_access_iterator_tag;

        Iterator(const CircularBuffer* buf, size_t idx) noexcept
            : buf_(buf), idx_(idx) {}

        reference operator*() const noexcept { return (*buf_)[idx_]; }
        pointer operator->() const noexcept { return &(*buf_)[idx_]; }

        Iterator& operator++() noexcept { ++idx_; return *this; }
        Iterator operator++(int) noexcept { Iterator tmp = *this; ++idx_; return tmp; }
        Iterator& operator--() noexcept { --idx_; return *this; }
        Iterator operator--(int) noexcept { Iterator tmp = *this; --idx_; return tmp; }

        Iterator operator+(difference_type n) const noexcept { return Iterator(buf_, idx_ + n); }
        Iterator operator-(difference_type n) const noexcept { return Iterator(buf_, idx_ - n); }
        difference_type operator-(const Iterator& other) const noexcept {
            return static_cast<difference_type>(idx_) - static_cast<difference_type>(other.idx_);
        }

        Iterator& operator+=(difference_type n) noexcept { idx_ += n; return *this; }
        Iterator& operator-=(difference_type n) noexcept { idx_ -= n; return *this; }

        reference operator[](difference_type n) const noexcept { return (*buf_)[idx_ + n]; }

        bool operator==(const Iterator& other) const noexcept { return idx_ == other.idx_; }
        bool operator!=(const Iterator& other) const noexcept { return idx_ != other.idx_; }
        bool operator<(const Iterator& other) const noexcept { return idx_ < other.idx_; }
        bool operator>(const Iterator& other) const noexcept { return idx_ > other.idx_; }
        bool operator<=(const Iterator& other) const noexcept { return idx_ <= other.idx_; }
        bool operator>=(const Iterator& other) const noexcept { return idx_ >= other.idx_; }

    private:
        const CircularBuffer* buf_;
        size_t idx_;
    };

    Iterator begin() const noexcept { return Iterator(this, 0); }
    Iterator end() const noexcept { return Iterator(this, count_); }

private:
    std::unique_ptr<T[]> buffer_;
    size_t write_pos_ = 0;
    size_t count_ = 0;
};

} // namespace trading
