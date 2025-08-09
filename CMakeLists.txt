cmake_minimum_required(VERSION 3.20)
project(ultra_low_latency_trading_system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release flags: maximum performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Common warning flags
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Fetch Google Test and Google Benchmark
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest googlebenchmark)

# Core library
add_library(trading_core STATIC
    src/config.cpp
    src/logger.cpp
    src/order_book.cpp
    src/fix_parser.cpp
    src/market_data_handler.cpp
    src/feed_simulator.cpp
    src/market_maker.cpp
    src/pairs_trading.cpp
    src/momentum.cpp
    src/exchange_simulator.cpp
    src/order_router.cpp
    src/execution_engine.cpp
    src/risk_manager.cpp
    src/position_tracker.cpp
    src/latency_tracker.cpp
    src/metrics_collector.cpp
)
target_include_directories(trading_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(trading_core PUBLIC pthread)

# Main executable
add_executable(trading_system src/main.cpp)
target_link_libraries(trading_system PRIVATE trading_core)

# Enable testing
enable_testing()

# Helper function for unit tests
function(add_unit_test name)
    add_executable(${name} tests/unit/${name}.cpp)
    target_link_libraries(${name} PRIVATE trading_core GTest::gtest_main)
    target_compile_definitions(${name} PRIVATE TESTING=1)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Helper function for integration tests
function(add_integration_test name)
    add_executable(${name} tests/integration/${name}.cpp)
    target_link_libraries(${name} PRIVATE trading_core GTest::gtest_main)
    target_compile_definitions(${name} PRIVATE TESTING=1)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Helper function for benchmarks
function(add_benchmark name)
    add_executable(${name} tests/benchmarks/${name}.cpp)
    target_link_libraries(${name} PRIVATE trading_core benchmark::benchmark benchmark::benchmark_main)
endfunction()

# Unit tests
add_unit_test(test_types)
add_unit_test(test_lock_free_queue)
add_unit_test(test_memory_pool)
add_unit_test(test_circular_buffer)
add_unit_test(test_order_book)
add_unit_test(test_fix_parser)
add_unit_test(test_market_data_handler)
add_unit_test(test_feed_simulator)
add_unit_test(test_market_maker)
add_unit_test(test_pairs_trading)
add_unit_test(test_momentum)
add_unit_test(test_exchange_simulator)
add_unit_test(test_order_router)
add_unit_test(test_execution_engine)
add_unit_test(test_position_tracker)
add_unit_test(test_risk_manager)

# Integration tests
add_integration_test(test_end_to_end)
add_integration_test(test_threading)

# Benchmarks
add_benchmark(bench_lock_free_queue)
add_benchmark(bench_memory_pool)
add_benchmark(bench_order_book)
add_benchmark(bench_fix_parser)
add_benchmark(bench_market_data_handler)
add_benchmark(bench_strategies)
add_benchmark(bench_execution)
add_benchmark(bench_risk_manager)
add_benchmark(bench_end_to_end)
add_benchmark(bench_throughput)
