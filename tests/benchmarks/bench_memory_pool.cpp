#include <benchmark/benchmark.h>
#include "containers/memory_pool.hpp"

using namespace trading;

struct BenchObj {
    uint64_t data[4]; // 32 bytes
};

static void BM_PoolAllocDeallocate(benchmark::State& state) {
    MemoryPool<BenchObj, 65536> pool;
    for (auto _ : state) {
        BenchObj* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }
}
BENCHMARK(BM_PoolAllocDeallocate);

static void BM_PoolAllocBatch(benchmark::State& state) {
    const int batch_size = state.range(0);
    MemoryPool<BenchObj, 65536> pool;
    BenchObj* ptrs[1024];

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            ptrs[i] = pool.allocate();
        }
        for (int i = 0; i < batch_size; ++i) {
            pool.deallocate(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_PoolAllocBatch)->Range(1, 1024);

static void BM_NewDeleteBaseline(benchmark::State& state) {
    for (auto _ : state) {
        BenchObj* ptr = new BenchObj;
        benchmark::DoNotOptimize(ptr);
        delete ptr;
    }
}
BENCHMARK(BM_NewDeleteBaseline);

static void BM_NewDeleteBatch(benchmark::State& state) {
    const int batch_size = state.range(0);
    BenchObj* ptrs[1024];

    for (auto _ : state) {
        for (int i = 0; i < batch_size; ++i) {
            ptrs[i] = new BenchObj;
        }
        for (int i = 0; i < batch_size; ++i) {
            delete ptrs[i];
        }
    }
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_NewDeleteBatch)->Range(1, 1024);

BENCHMARK_MAIN();
