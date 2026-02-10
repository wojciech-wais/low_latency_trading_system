---
title: "System Architecture"
subtitle: "Six-stage pipeline with lock-free inter-thread communication"
---

## Pipeline Overview

The system is a pipeline of six components connected by lock-free SPSC (Single-Producer Single-Consumer) queues. Each component can be pinned to a dedicated CPU core for maximum throughput and minimum latency.

<div class="diagram">
<pre>
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
</pre>
</div>

## Data Flow

1. **FIX messages** arrive from the market data feed
2. **Zero-copy parser** extracts fields via `string_view` into the raw buffer
3. Parsed messages pass through a **lock-free SPSC queue** to the order book
4. **Order book** updates price levels using price-time priority
5. Updated book state triggers **strategy signal generation**
6. Signals pass through the **risk manager** for pre-trade validation (~20ns)
7. Approved orders route to the **execution engine** via another SPSC queue
8. Execution reports feed back for position tracking

## Threading Model

| Thread | Core | Components | Communication |
|--------|------|-----------|---------------|
| Main | 2 | MD Handler + Order Book + Strategy + Risk | Produces to order queue |
| Execution | 8 | Execution Engine | Consumes order queue, produces exec reports |
| Logger | any | Background log drain | Consumes log queue |

The main hot loop runs market data parsing, order book updates, strategy signal generation, and risk checks **on a single thread** to minimize queue hops. The execution engine runs on a separate thread since it simulates exchange latency.

## Key Data Structures

### Order Book

- **Bids**: `std::map<Price, PriceLevel, std::greater<>>` (descending by price)
- **Asks**: `std::map<Price, PriceLevel>` (ascending by price)
- **Order lookup**: `std::unordered_map<OrderId, OrderBookEntry*>` for O(1) cancel
- **Price levels**: intrusive doubly-linked list of orders (O(1) insert/remove)
- **Trade output**: `std::span` over `thread_local static` array (zero allocation)

### Fixed-Point Prices

All prices stored as `int64_t` with 2 decimal places. For example, $150.50 is stored as `15050`. This eliminates floating-point overhead and enables exact comparison — critical for price-time priority matching.

### Lock-Free SPSC Ring Buffer

- Power-of-2 capacity for efficient index masking (`index & (Capacity - 1)`)
- Producer: write data, then `store(tail, memory_order_release)`
- Consumer: `load(tail, memory_order_acquire)`, then read data
- Head and tail on **separate cache lines** (`alignas(64)`) to prevent false sharing
- Heap-allocated buffer (single allocation at construction, not on hot path)

### Memory Pool

- Contiguous pre-allocated array (single heap allocation at startup)
- Index-based intrusive free list
- O(1) allocate: pop from free list head
- O(1) deallocate: push to free list head
- Single-threaded — no atomics overhead needed

## Design Decisions

<div class="component-grid">
  <div class="component-card">
    <h4>Cache-Line Alignment</h4>
    <p><code>alignas(64)</code> on all hot data structures prevents false sharing between CPU cores. Head and tail indices of the SPSC queue are on separate cache lines.</p>
  </div>
  <div class="component-card">
    <h4>Acquire/Release Ordering</h4>
    <p>Uses the minimum necessary memory ordering. <code>seq_cst</code> is never used — it adds unnecessary overhead. Own-index loads use <code>relaxed</code> since only one thread writes.</p>
  </div>
  <div class="component-card">
    <h4>Zero-Copy Parsing</h4>
    <p>FIX parser uses <code>string_view</code> into the raw buffer. Tag lookup via a flat array indexed by tag number gives O(1) field access.</p>
  </div>
  <div class="component-card">
    <h4>No Exceptions</h4>
    <p>Compiled with <code>-fno-exceptions -fno-rtti</code>. All error handling via return codes. Eliminates exception table overhead and enables better code generation.</p>
  </div>
</div>
