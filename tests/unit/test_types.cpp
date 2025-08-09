#include <gtest/gtest.h>
#include "common/types.hpp"

using namespace trading;

TEST(TypesTest, OrderSizeIsCacheLine) {
    static_assert(sizeof(Order) == 64, "Order must be 64 bytes");
    EXPECT_EQ(sizeof(Order), CACHE_LINE_SIZE);
}

TEST(TypesTest, OrderAlignment) {
    static_assert(alignof(Order) == 64, "Order must be cache-line aligned");
}

TEST(TypesTest, FixedPriceConversion) {
    EXPECT_EQ(to_fixed_price(150.50), 15050);
    EXPECT_EQ(to_fixed_price(0.01), 1);
    EXPECT_EQ(to_fixed_price(100.00), 10000);
    EXPECT_EQ(to_fixed_price(99.99), 9999);
    EXPECT_EQ(to_fixed_price(0.0), 0);
}

TEST(TypesTest, FixedPriceRoundTrip) {
    for (double price : {0.01, 1.00, 50.50, 100.00, 150.25, 999.99}) {
        Price fixed = to_fixed_price(price);
        double recovered = to_double_price(fixed);
        EXPECT_NEAR(recovered, price, 0.005);
    }
}

TEST(TypesTest, FixedPriceNegative) {
    EXPECT_EQ(to_fixed_price(-10.50), -1050);
    EXPECT_NEAR(to_double_price(-1050), -10.50, 0.005);
}

TEST(TypesTest, NowNsReturnsIncreasingValues) {
    Timestamp t1 = now_ns();
    Timestamp t2 = now_ns();
    EXPECT_GE(t2, t1);
}

TEST(TypesTest, NowNsNonZero) {
    EXPECT_GT(now_ns(), 0u);
}

TEST(TypesTest, OppositeSide) {
    EXPECT_EQ(opposite_side(Side::Buy), Side::Sell);
    EXPECT_EQ(opposite_side(Side::Sell), Side::Buy);
}

TEST(TypesTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(Side::Buy), 0);
    EXPECT_EQ(static_cast<uint8_t>(Side::Sell), 1);
    EXPECT_EQ(static_cast<uint8_t>(OrderType::Limit), 0);
    EXPECT_EQ(static_cast<uint8_t>(OrderType::Market), 1);
    EXPECT_EQ(static_cast<uint8_t>(OrderType::IOC), 2);
    EXPECT_EQ(static_cast<uint8_t>(OrderType::FOK), 3);
    EXPECT_EQ(static_cast<uint8_t>(OrderStatus::New), 0);
    EXPECT_EQ(static_cast<uint8_t>(OrderStatus::Filled), 2);
}

TEST(TypesTest, PriceScaleConstant) {
    EXPECT_EQ(PRICE_SCALE, 100);
}

TEST(TypesTest, TradeStructSize) {
    // Trade doesn't need to be exactly 64 bytes but should be reasonable
    EXPECT_LE(sizeof(Trade), 64u);
}
