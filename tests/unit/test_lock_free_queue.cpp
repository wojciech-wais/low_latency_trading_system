#include <gtest/gtest.h>
#include "containers/lock_free_queue.hpp"
#include <thread>
#include <vector>
#include <numeric>

using namespace trading;

TEST(LockFreeQueueTest, InitiallyEmpty) {
    LockFreeRingBuffer<int, 16> queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);
    EXPECT_FALSE(queue.full());
}

TEST(LockFreeQueueTest, PushPop) {
    LockFreeRingBuffer<int, 16> queue;
    EXPECT_TRUE(queue.try_push(42));
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_FALSE(queue.empty());

    int val = 0;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(queue.empty());
}

TEST(LockFreeQueueTest, FIFO) {
    LockFreeRingBuffer<int, 64> queue;
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(queue.try_push(i));
    }
    for (int i = 0; i < 10; ++i) {
        int val = -1;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(LockFreeQueueTest, FullQueue) {
    LockFreeRingBuffer<int, 4> queue; // Usable capacity = 3
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_FALSE(queue.try_push(4)); // Should fail - full
    EXPECT_TRUE(queue.full());
}

TEST(LockFreeQueueTest, EmptyPop) {
    LockFreeRingBuffer<int, 16> queue;
    int val = 42;
    EXPECT_FALSE(queue.try_pop(val));
    EXPECT_EQ(val, 42); // Unchanged
}

TEST(LockFreeQueueTest, WrapAround) {
    LockFreeRingBuffer<int, 4> queue; // Cap = 3
    // Fill and drain multiple times to wrap around
    for (int cycle = 0; cycle < 10; ++cycle) {
        EXPECT_TRUE(queue.try_push(cycle * 10 + 1));
        EXPECT_TRUE(queue.try_push(cycle * 10 + 2));
        EXPECT_TRUE(queue.try_push(cycle * 10 + 3));

        int val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, cycle * 10 + 1);
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, cycle * 10 + 2);
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, cycle * 10 + 3);
        EXPECT_TRUE(queue.empty());
    }
}

TEST(LockFreeQueueTest, Capacity) {
    LockFreeRingBuffer<int, 64> queue;
    EXPECT_EQ(queue.capacity(), 63u);
}

TEST(LockFreeQueueTest, StructTransport) {
    struct Data {
        uint64_t a;
        uint64_t b;
        double c;
    };
    LockFreeRingBuffer<Data, 16> queue;
    Data in{42, 99, 3.14};
    EXPECT_TRUE(queue.try_push(in));

    Data out{};
    EXPECT_TRUE(queue.try_pop(out));
    EXPECT_EQ(out.a, 42u);
    EXPECT_EQ(out.b, 99u);
    EXPECT_DOUBLE_EQ(out.c, 3.14);
}

TEST(LockFreeQueueTest, TwoThreadStress) {
    constexpr size_t NUM_ITEMS = 1'000'000;
    LockFreeRingBuffer<uint64_t, 65536> queue;

    std::atomic<uint64_t> sum_produced{0};
    std::atomic<uint64_t> sum_consumed{0};
    std::atomic<bool> done{false};

    // Producer thread
    std::thread producer([&]() {
        uint64_t local_sum = 0;
        for (uint64_t i = 1; i <= NUM_ITEMS; ++i) {
            while (!queue.try_push(i)) {
                // Spin
            }
            local_sum += i;
        }
        sum_produced.store(local_sum, std::memory_order_relaxed);
        done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        uint64_t local_sum = 0;
        uint64_t count = 0;
        uint64_t expected = 1;
        while (count < NUM_ITEMS) {
            uint64_t val;
            if (queue.try_pop(val)) {
                EXPECT_EQ(val, expected) << "FIFO violation at item " << count;
                expected++;
                local_sum += val;
                ++count;
            }
        }
        sum_consumed.store(local_sum, std::memory_order_relaxed);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(sum_produced.load(), sum_consumed.load());
    // Sum of 1..N = N*(N+1)/2
    uint64_t expected_sum = NUM_ITEMS * (NUM_ITEMS + 1) / 2;
    EXPECT_EQ(sum_consumed.load(), expected_sum);
}
