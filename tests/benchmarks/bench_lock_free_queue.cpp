#include <benchmark/benchmark.h>
#include "containers/lock_free_queue.hpp"
#include "common/types.hpp"
#include <thread>

using namespace trading;

static void BM_QueuePushPop_SingleThread(benchmark::State& state) {
    LockFreeRingBuffer<uint64_t, 65536> queue;
    uint64_t val = 0;
    for (auto _ : state) {
        queue.try_push(val);
        queue.try_pop(val);
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_QueuePushPop_SingleThread);

static void BM_QueuePush_SingleThread(benchmark::State& state) {
    LockFreeRingBuffer<uint64_t, 65536> queue;
    uint64_t val = 0;
    for (auto _ : state) {
        if (!queue.try_push(val++)) {
            uint64_t tmp;
            queue.try_pop(tmp);
            queue.try_push(val);
        }
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_QueuePush_SingleThread);

static void BM_QueueOrderTransport(benchmark::State& state) {
    LockFreeRingBuffer<Order, 65536> queue;
    Order order{};
    order.id = 1;
    order.price = 15000;
    order.quantity = 100;
    for (auto _ : state) {
        queue.try_push(order);
        Order out;
        queue.try_pop(out);
        benchmark::DoNotOptimize(out);
    }
}
BENCHMARK(BM_QueueOrderTransport);

static void BM_QueueTwoThread(benchmark::State& state) {
    LockFreeRingBuffer<uint64_t, 65536> queue;
    std::atomic<bool> running{true};
    std::atomic<uint64_t> consumed{0};

    std::thread consumer([&]() {
        uint64_t val;
        while (running.load(std::memory_order_relaxed)) {
            if (queue.try_pop(val)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // Drain
        while (queue.try_pop(val)) {
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    uint64_t pushed = 0;
    for (auto _ : state) {
        while (!queue.try_push(pushed)) {}
        ++pushed;
    }

    running.store(false, std::memory_order_relaxed);
    consumer.join();

    state.SetItemsProcessed(static_cast<int64_t>(pushed));
}
BENCHMARK(BM_QueueTwoThread)->UseRealTime();

BENCHMARK_MAIN();
