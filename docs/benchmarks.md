# Benchmark Results

All measurements taken on Linux with Release build (`-O3 -march=native -flto`).

## Latency (Release Build)

| Component | p50 | p90 | p95 | p99 | p99.9 | Max |
|-----------|-----|-----|-----|-----|-------|-----|
| Market Data Parse | 702ns | 762ns | 791ns | 881ns | 1172ns | 5580ns |
| Order Book Update | 20ns | 20ns | 20ns | 21ns | 21ns | 1773ns |
| Strategy Signal | 891ns | 932ns | 932ns | 942ns | 1022ns | 10720ns |
| Risk Check | 20ns | 20ns | 20ns | 21ns | 21ns | 2424ns |
| Tick-to-Trade | 1603ns | 1703ns | 1723ns | 1793ns | 2215ns | 11482ns |

## Throughput (Release Build, 10s simulation)

| Metric | Rate |
|--------|------|
| Market data messages | 656K msgs/sec |
| Order book updates | 656K updates/sec |
| Orders sent | 12 orders/sec |
| Fills | 6 fills/sec |

## Tick-to-Trade Distribution

```
  0-10ns   |        0 (  0.0%)
 10-100ns  |        0 (  0.0%)
100ns-1us  |        0 (  0.0%)
  1-10us   |  6562642 (100.0%) #################################################
 10-100us  |       73 (  0.0%)
100us-1ms  |      194 (  0.0%)
  >1ms     |        0 (  0.0%)
```

99.99%+ of ticks processed in under 10 microseconds.

## Container Benchmarks

### Lock-Free SPSC Queue
- Push+Pop (single thread): ~30ns per round trip
- Two-thread throughput: >10M items/sec

### Memory Pool
- Allocate+Deallocate: ~5ns per round trip
- 10-100x faster than `new`/`delete`

### FIX Parser
- Parse + extract fields: ~400ns per message
- >2M parses/sec

## Debug vs Release Comparison

| Metric | Debug | Release | Speedup |
|--------|-------|---------|---------|
| Tick-to-Trade p50 | 11.1us | 1.6us | 6.9x |
| Market Data p50 | 2.0us | 0.7us | 2.9x |
| Throughput | 81K/s | 656K/s | 8.1x |
