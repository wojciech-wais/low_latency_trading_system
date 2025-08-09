#include <benchmark/benchmark.h>
#include "market_data/fix_parser.hpp"

using namespace trading;

static const std::string SAMPLE_MSG =
    "8=FIX.4.4|9=200|35=W|49=FEED|56=CLIENT|34=12345|"
    "55=AAPL|132=150.50|133=150.75|134=500|135=300|44=150.60|38=50|10=000|";

static void BM_FixParserParse(benchmark::State& state) {
    FixParser parser;
    for (auto _ : state) {
        parser.parse(SAMPLE_MSG);
        benchmark::DoNotOptimize(parser.msg_type());
    }
}
BENCHMARK(BM_FixParserParse);

static void BM_FixParserParseAndExtract(benchmark::State& state) {
    FixParser parser;
    for (auto _ : state) {
        parser.parse(SAMPLE_MSG);
        auto symbol = parser.get_symbol();
        auto bid = parser.get_bid_price();
        auto ask = parser.get_ask_price();
        benchmark::DoNotOptimize(symbol);
        benchmark::DoNotOptimize(bid);
        benchmark::DoNotOptimize(ask);
    }
}
BENCHMARK(BM_FixParserParseAndExtract);

static void BM_FixParserNewOrderSingle(benchmark::State& state) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|9=100|35=D|49=CLIENT|56=EXCHANGE|34=1|"
                      "11=12345|55=AAPL|54=1|38=100|40=2|44=150.50|10=123|";
    for (auto _ : state) {
        parser.parse(msg);
        auto id = parser.get_order_id();
        auto price = parser.get_price();
        auto qty = parser.get_quantity();
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(price);
        benchmark::DoNotOptimize(qty);
    }
}
BENCHMARK(BM_FixParserNewOrderSingle);

BENCHMARK_MAIN();
