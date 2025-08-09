# Performance Tuning Guide

## Compiler Flags

### Release Build
```
-std=c++20 -O3 -march=native -flto -DNDEBUG -fno-exceptions -fno-rtti
```

- `-O3`: Aggressive optimizations including vectorization
- `-march=native`: Use all CPU-specific instructions (AVX2, etc.)
- `-flto`: Link-time optimization for cross-TU inlining
- `-fno-exceptions -fno-rtti`: Eliminate exception/RTTI overhead

### Debug Build
```
-std=c++20 -g -O0
```

For ThreadSanitizer: `-fsanitize=thread`

## CPU Affinity

Pin threads to even-numbered cores to avoid SMT sibling interference:

```cpp
pin_thread_to_core(2);  // Market data
pin_thread_to_core(8);  // Execution
```

Check your topology: `lscpu --extended` or `cat /proc/cpuinfo`

## Cache Optimization

- All shared structures are `alignas(64)` to prevent false sharing
- SPSC queue head/tail are on separate cache lines
- Order struct fits in a single 64-byte cache line
- Price level uses intrusive linked list (no pointer chasing through allocator)

## Profiling

### perf
```bash
perf stat ./build-release/trading_system
perf record -g ./build-release/trading_system
perf report
```

### heaptrack (verify zero hot-path allocations)
```bash
heaptrack ./build-release/trading_system
heaptrack_print heaptrack.trading_system.*.gz
```

### Verify hot path has no function calls
```bash
objdump -d build-release/libtrading_core.a | grep -A 50 "check_order"
```

## Tuning Parameters

Edit `config/system_config.json`:

| Parameter | Default | Notes |
|-----------|---------|-------|
| `market_data_queue_size` | 65536 | Must be power of 2 |
| `max_orders_per_second` | 10000 | Rate limiter |
| `simulation_duration_ms` | 10000 | Simulation length |
| `volatility` | 0.001 | Per-tick price volatility |
| `market_maker_spread_bps` | 10.0 | Base spread |
