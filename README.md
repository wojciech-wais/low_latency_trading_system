# Ultra-Low Latency HFT Trading Simulator

A production-grade High-Frequency Trading (HFT) simulator built in modern C++20, demonstrating sub-microsecond order processing, lock-free data structures, and zero hot-path heap allocations.

## Performance Results

| Metric | Target | Achieved |
|--------|--------|----------|
| Tick-to-Trade (p50) | < 5 us | **1.6 us** |
| Tick-to-Trade (p99) | < 20 us | **1.8 us** |
| Risk Check (p99) | < 100 ns | **21 ns** |
| Order Book BBO | < 10 ns | **20 ns** |
| Market Data Parse | < 500 ns | **702 ns** |
| Market Data Throughput | 1M msgs/sec | **656K msgs/sec** (full pipeline) |
| Book Updates | 500K/sec | **656K/sec** |

### Benchmark Environment

| Component | Specification |
|-----------|--------------|
| **CPU** | AMD Ryzen 9 7900X (12 cores / 24 threads, up to 5.73 GHz boost) |
| **Architecture** | Zen 4, x86_64 |
| **L1 Cache** | 384 KiB I + 384 KiB D (per-core: 32 KiB each) |
| **L2 Cache** | 12 MiB (1 MiB per core) |
| **L3 Cache** | 64 MiB (2 x 32 MiB CCDs) |
| **RAM** | 64 GB DDR5 |
| **OS** | Ubuntu 22.04.5 LTS |
| **Kernel** | 6.5.0-26-generic |
| **Compiler** | GCC 11.4.0 |
| **Build Flags** | `-std=c++20 -O3 -march=native -flto -fno-exceptions -fno-rtti` |

## Architecture

```
Market Data Feed (FIX Protocol)
      |
      v
[Market Data Handler] --SPSC queue--> [Order Book Engine]
      (Core 2)                              (Core 4)
                                                |
                                                v
                                       [Strategy Engine] --> [Execution Engine]
                                          (Core 6)              (Core 8)
                                                |
                                          [Risk Manager]
                                        (pre-trade checks)
                                                |
                                       [Performance Monitor]
                                          (Core 10)
```

**Six core components** connected via lock-free SPSC queues, each pinnable to a dedicated CPU core.

## Key Design Decisions

- **Fixed-point prices**: `int64_t` with 2 decimal places (150.50 -> 15050) avoids floating-point overhead
- **Cache-line alignment**: `alignas(64)` on hot data structures prevents false sharing
- **Memory pool allocators**: O(1) allocate/deallocate, pre-allocated at startup
- **Lock-free SPSC ring buffers**: acquire/release memory ordering, power-of-2 masking
- **Zero-copy FIX parsing**: `string_view` into raw buffer, O(1) tag lookup via flat array
- **Pre-allocated order buffers**: strategies return `std::span` over member arrays

## Components

### Market Data Handler
- Zero-copy FIX protocol parser (~700ns per message)
- Feed simulator with random walk pricing and CSV replay
- Lock-free queue output to downstream consumers

### Order Book Engine
- Price-time priority matching
- O(1) cancel via intrusive doubly-linked lists
- Supports Limit, Market, IOC, FOK order types
- Thread-local trade buffer (no heap allocation on match)

### Strategy Engine
- **Market Making**: dynamic spread based on volatility, inventory skew, aggressive flatten at limits
- **Pairs Trading**: z-score based entry/exit on spread of two instruments
- **Momentum**: fast/slow EMA crossover with breakout threshold

### Execution Engine
- Smart order routing across multiple simulated exchanges
- Configurable latency profiles and fill probabilities
- Token bucket rate limiting
- Order state machine (New -> Ack -> Filled/Cancelled/Rejected)

### Risk Manager
- Pre-trade checks completing in ~20ns (target was <100ns)
- Kill switch, position limits, capital limits, order size, rate limit, fat finger
- Flat array position tracking (O(1) by instrument ID)
- Multiplication instead of division for percentage checks

### Performance Monitor
- Latency histograms (p50/p90/p95/p99/p99.9/max)
- Log-scale histogram visualization
- Throughput counters for all pipeline stages

## Quick Start

### Prerequisites
- GCC 11+ or Clang 14+ (C++20)
- CMake 3.20+
- Linux (for thread pinning)

### Build and Run

```bash
# Configure and build (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run the simulator
./build/trading_system

# Run with custom config
./build/trading_system config/system_config.json

# Run tests
cd build && ctest --output-on-failure

# Run benchmarks
./build/bench_risk_manager
./build/bench_order_book
./build/bench_fix_parser
```

### Debug Build

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)
cd build-debug && ctest --output-on-failure
```

## Project Structure

```
include/
  common/         types.hpp, config.hpp, logger.hpp, utils.hpp
  containers/     lock_free_queue.hpp, memory_pool.hpp, circular_buffer.hpp
  market_data/    fix_parser.hpp, market_data_handler.hpp, feed_simulator.hpp
  order_book/     order.hpp, price_level.hpp, order_book.hpp
  strategy/       strategy_interface.hpp, market_maker.hpp, pairs_trading.hpp, momentum.hpp
  execution/      exchange_simulator.hpp, order_router.hpp, execution_engine.hpp
  risk/           risk_manager.hpp, position_tracker.hpp
  monitoring/     latency_tracker.hpp, histogram.hpp, metrics_collector.hpp
src/              implementations + main.cpp
tests/
  unit/           16 unit test suites
  integration/    end-to-end and threading tests
  benchmarks/     10 benchmark suites
config/           system_config.json
data/             sample_market_data.csv
```

## Testing

- **18 test suites** covering all components
- Unit tests for containers, order book, FIX parser, strategies, execution, risk
- Integration tests: full pipeline end-to-end, multi-threaded shutdown
- Stress tests: 1M item queue transport, 100K random order book operations

## Dependencies

- Google Test (fetched via CMake FetchContent)
- Google Benchmark (fetched via CMake FetchContent)
- POSIX threads (pthread)
- No external runtime dependencies

## License

MIT
