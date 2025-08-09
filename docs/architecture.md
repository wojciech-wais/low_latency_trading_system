# Architecture

## System Overview

The trading system is a pipeline of six components connected by lock-free SPSC queues. Each component can be pinned to a dedicated CPU core for maximum throughput and minimum latency.

## Data Flow

```
FIX Market Data
      |
      v
[FIX Parser] --> [MarketDataMessage]
      |
      v (SPSC Queue)
[Order Book Update] --> [Strategy Signal Generation]
      |
      v
[Risk Check] --> approved/rejected
      |
      v (SPSC Queue)
[Execution Engine] --> [Exchange Simulator]
      |
      v (SPSC Queue)
[Execution Report] --> feedback to strategies
```

## Threading Model

| Thread | Core | Component | Communication |
|--------|------|-----------|---------------|
| Main | 2 | MD Handler + OB + Strategy + Risk | Produces to order_queue |
| Exec | 8 | Execution Engine | Consumes order_queue, produces exec_report_queue |
| Logger | any | Background log drain | Consumes log queue |

The main hot loop runs market data parsing, order book updates, strategy signal generation, and risk checks on a single thread to minimize queue hops. The execution engine runs on a separate thread since it simulates exchange latency.

## Lock-Free Design

### SPSC Ring Buffer
- Power-of-2 capacity for efficient index masking
- Producer stores data then does `release` store on tail
- Consumer does `acquire` load on tail, then reads data
- Own index loaded with `relaxed` ordering
- Head and tail on separate cache lines (`alignas(64)`) to prevent false sharing

### Memory Pool
- Contiguous pre-allocated array (single heap allocation at startup)
- Index-based intrusive free list
- O(1) allocate: pop from free list head
- O(1) deallocate: push to free list head
- Single-threaded (no atomics overhead)

## Key Data Structures

### Order Book
- **Bids**: `std::map<Price, PriceLevel, std::greater<>>` (descending)
- **Asks**: `std::map<Price, PriceLevel>` (ascending)
- **Order lookup**: `std::unordered_map<OrderId, OrderBookEntry*>` for O(1) cancel
- **Price level**: intrusive doubly-linked list of orders (O(1) insert/remove)
- **Trades**: returned via `std::span` over `thread_local static` array (no allocation)

### Fixed-Point Prices
All prices stored as `int64_t` with 2 decimal places (e.g., $150.50 = 15050). This eliminates floating-point overhead and enables exact comparison.

## Risk Manager Design
The risk manager performs 6 sequential checks in ~20ns:

1. Kill switch (atomic load)
2. Order size (single comparison)
3. Position limit (array lookup + abs comparison)
4. Capital limit (multiplication)
5. Order rate (counter + comparison)
6. Fat finger (multiplication instead of division)

`[[unlikely]]` on all failure branches enables the compiler to optimize the approved path as straight-line code.
