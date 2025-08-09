#include <gtest/gtest.h>
#include "market_data/fix_parser.hpp"

using namespace trading;

TEST(FixParserTest, ParseNewOrderSingle) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|9=100|35=D|49=CLIENT|56=EXCHANGE|34=1|"
                      "11=12345|55=AAPL|54=1|38=100|40=2|44=150.50|10=123|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_TRUE(parser.valid());

    EXPECT_EQ(parser.msg_type(), "D");
    EXPECT_EQ(parser.get_symbol(), "AAPL");
    EXPECT_EQ(parser.get_order_id(), 12345u);
    EXPECT_EQ(parser.get_side(), Side::Buy);
    EXPECT_EQ(parser.get_quantity(), 100u);
    EXPECT_EQ(parser.get_price(), 15050);
    EXPECT_EQ(parser.get_order_type(), OrderType::Limit);
}

TEST(FixParserTest, ParseMarketDataSnapshot) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|9=200|35=W|49=FEED|56=CLIENT|34=1|"
                      "55=GOOG|132=145.50|133=145.75|134=500|135=300|44=145.60|38=50|10=000|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.msg_type(), "W");
    EXPECT_EQ(parser.get_symbol(), "GOOG");
    EXPECT_EQ(parser.get_bid_price(), 14550);
    EXPECT_EQ(parser.get_ask_price(), 14575);
    EXPECT_EQ(parser.get_bid_size(), 500u);
    EXPECT_EQ(parser.get_ask_size(), 300u);
}

TEST(FixParserTest, ParseExecutionReport) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|9=150|35=8|49=EXCHANGE|56=CLIENT|34=1|"
                      "11=12345|55=AAPL|54=1|38=100|44=150.50|10=123|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.msg_type(), "8");
}

TEST(FixParserTest, FieldLookup) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|35=D|49=CLIENT|56=EXCHANGE|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.get_field(8), "FIX.4.4");
    EXPECT_EQ(parser.get_field(49), "CLIENT");
    EXPECT_EQ(parser.get_field(56), "EXCHANGE");
    EXPECT_TRUE(parser.get_field(99).empty()); // Non-existent tag
}

TEST(FixParserTest, ZeroCopy) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|35=D|55=AAPL|";
    EXPECT_TRUE(parser.parse(msg));

    // Verify that returned string_view points into original buffer
    auto symbol = parser.get_symbol();
    EXPECT_EQ(symbol, "AAPL");
    // The data pointer should be within the original message
    EXPECT_GE(symbol.data(), msg.data());
    EXPECT_LT(symbol.data(), msg.data() + msg.size());
}

TEST(FixParserTest, EmptyMessage) {
    FixParser parser;
    EXPECT_FALSE(parser.parse(""));
    EXPECT_FALSE(parser.valid());
}

TEST(FixParserTest, InvalidMessage) {
    FixParser parser;
    EXPECT_FALSE(parser.parse("not a fix message"));
    EXPECT_FALSE(parser.valid());
}

TEST(FixParserTest, Reset) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|35=D|55=AAPL|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_TRUE(parser.valid());

    parser.reset();
    EXPECT_FALSE(parser.valid());
    EXPECT_TRUE(parser.get_field(35).empty());
}

TEST(FixParserTest, PriceWithDecimals) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|35=D|44=99.99|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.get_price(), 9999);
}

TEST(FixParserTest, SellSide) {
    FixParser parser;
    std::string msg = "8=FIX.4.4|35=D|54=2|";
    EXPECT_TRUE(parser.parse(msg));
    EXPECT_EQ(parser.get_side(), Side::Sell);
}

TEST(FixParserTest, OrderTypes) {
    FixParser parser;

    parser.parse("8=FIX.4.4|35=D|40=1|");
    EXPECT_EQ(parser.get_order_type(), OrderType::Market);

    parser.parse("8=FIX.4.4|35=D|40=2|");
    EXPECT_EQ(parser.get_order_type(), OrderType::Limit);

    parser.parse("8=FIX.4.4|35=D|40=3|");
    EXPECT_EQ(parser.get_order_type(), OrderType::IOC);

    parser.parse("8=FIX.4.4|35=D|40=4|");
    EXPECT_EQ(parser.get_order_type(), OrderType::FOK);
}
