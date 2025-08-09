# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Ultra-low latency HFT simulator in C++20/23. Target: sub-microsecond order processing, 1M+ market data messages/sec, zero heap allocations on hot paths. See `ULTRA_LOW_LATENCY_TRADING_SYSTEM_SPEC.md` for the full specification.

## Build System

```bash
# Configure (from project root)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Debug build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)

# Run tests (Google Test)
cd build && ctest --output-on-failure

# Run a single test
./build/tests/unit/<test_binary> --gtest_filter="TestSuite.TestName"

# Run benchmarks (Google Benchmark)
./build/tests/benchmarks/<benchmark_binary>
```

**Compiler flags (Release):** `-std=c++20 -O3 -march=native -flto -DNDEBUG -fno-exceptions -fno-rtti`

## Architecture

Six core components connected via lock-free SPSC queues, each pinned to a dedicated CPU core:

```
Market Data Feed
      │
      ▼
[Market Data Handler] ──lock-free queue──▶ [Order Book Engine]
      (Core 0)                                  (Core 1)
                                                    │
                                                    ▼
                                           [Strategy Engine] ──▶ [Execution Engine]
                                              (Core 2)              (Core 3)
                                                    │
                                              [Risk Manager]
                                            (pre-trade checks)
                                                    │
                                           [Performance Monitor]
                                              (Core 4)
```

**Data flow:** Market data → FIX parser (zero-copy) → order book reconstruction → strategy signal generation → risk check (<100ns) → order routing to simulated exchanges.

### Key Design Decisions

- **Fixed-point prices** (`int64_t`): `150.50` stored as `15050` to avoid floating-point overhead
- **Cache-line alignment** (`alignas(64)`) on all hot data structures to prevent false sharing
- **Memory pool allocators** for orders — no `new`/`delete` on hot paths
- **Lock-free SPSC ring buffers** for inter-thread communication (acquire/release memory ordering)
- **Thread pinning** via `pthread_setaffinity_np` with NUMA-aware allocation

### Component Details

- **Market Data Handler** (`include/market_data/`): FIX protocol parser (simplified, zero-copy), feed simulator, mmap-based historical replay
- **Order Book Engine** (`include/order_book/`): Price-time priority matching. `std::map<Price, PriceLevel>` for price levels, `std::unordered_map<OrderId, Order*>` for O(1) lookup. Supports Limit, Market, IOC, FOK orders
- **Strategy Engine** (`include/strategy/`): Three strategies behind a common `StrategyInterface` — market making (with inventory/spread management), statistical arbitrage (pairs/z-score), momentum (rolling window breakout)
- **Execution Engine** (`include/execution/`): Smart order routing across multiple simulated exchanges with configurable latency profiles and fill simulation
- **Risk Manager** (`include/risk/`): Pre-trade checks (position limits, capital limits, order size, order rate, fat finger) — all must complete in <100ns
- **Performance Monitor** (`include/monitoring/`): Latency histograms (p50/p90/p95/p99/p99.9), throughput metrics, circular buffer storage

## Dependencies

- **Compiler:** GCC 11+ or Clang 14+ (C++20 required)
- **Build:** CMake 3.20+
- **Platform:** Linux (required for thread pinning, NUMA)
- **Testing:** Google Test, Google Benchmark
- **Optional:** spdlog (logging), rapidjson (config parsing)
- **Profiling:** perf, valgrind, heaptrack

## Namespace and Types

All code lives in the `trading` namespace. Core types are in `include/common/types.hpp`:
- `Price` = `int64_t`, `Quantity` = `uint64_t`, `OrderId` = `uint64_t`
- `InstrumentId` = `uint32_t`, `ExchangeId` = `uint8_t`, `Timestamp` = `uint64_t` (nanoseconds)
- Enums: `Side`, `OrderType`, `OrderStatus`

## Performance Constraints

When modifying hot-path code:
- No heap allocations (use memory pools)
- No locks (use lock-free structures with `std::atomic` + appropriate memory ordering)
- No exceptions on hot paths (`-fno-exceptions` is set globally)
- Use `[[likely]]`/`[[unlikely]]`, `__attribute__((hot))`, `__builtin_expect` for branch hints
- Use `constexpr` where possible for compile-time computation
- Maintain cache-line alignment (`alignas(64)`) on shared data
