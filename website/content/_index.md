---
title: "Ultra-Low Latency HFT Simulator"
---

## About This Project

This is a **production-grade High-Frequency Trading simulator** built from scratch in modern C++20. It demonstrates the full pipeline of a real HFT system: market data ingestion, order book reconstruction, strategy signal generation, risk management, and order execution — all optimized for sub-microsecond latency.

<div class="tech-badges">
  <span class="badge">C++20</span>
  <span class="badge">Lock-Free</span>
  <span class="badge">SPSC Queues</span>
  <span class="badge">Zero Allocation</span>
  <span class="badge">Linux / POSIX</span>
  <span class="badge">CMake</span>
  <span class="badge">Google Test</span>
  <span class="badge">Google Benchmark</span>
  <span class="badge">Cache-Aligned</span>
  <span class="badge">Fixed-Point Math</span>
</div>

## Highlights

- **Six-stage pipeline** connected by lock-free SPSC ring buffers, each pinnable to a dedicated CPU core
- **Zero heap allocations** on the hot path — all memory pre-allocated at startup via custom pool allocators
- **Sub-microsecond tick-to-trade** latency (1.6 &mu;s p50) with 21 ns risk checks
- **Three trading strategies**: market making with inventory skew, pairs trading with z-score signals, and momentum with EMA crossover
- **Smart order routing** across multiple simulated exchanges with configurable latency profiles
- **18 passing tests** (unit + integration) and 10 benchmark suites

## System at a Glance

<div class="component-grid">
  <div class="component-card">
    <h4>Market Data Handler</h4>
    <p>Zero-copy FIX protocol parser (~700ns/msg). Feed simulator with random walk pricing and CSV replay.</p>
  </div>
  <div class="component-card">
    <h4>Order Book Engine</h4>
    <p>Price-time priority matching. O(1) cancel via intrusive linked lists. Supports Limit, Market, IOC, FOK orders.</p>
  </div>
  <div class="component-card">
    <h4>Strategy Engine</h4>
    <p>Market making, pairs trading, and momentum strategies. Pre-allocated order buffers returned via std::span.</p>
  </div>
  <div class="component-card">
    <h4>Risk Manager</h4>
    <p>Six pre-trade checks in ~20ns. Kill switch, position limits, capital limits, rate limiting, fat finger detection.</p>
  </div>
  <div class="component-card">
    <h4>Execution Engine</h4>
    <p>Smart order routing across multiple exchanges. Token bucket rate limiting. Full order state machine.</p>
  </div>
  <div class="component-card">
    <h4>Performance Monitor</h4>
    <p>Latency histograms (p50 through p99.9). Throughput counters. Log-scale visualization.</p>
  </div>
</div>
