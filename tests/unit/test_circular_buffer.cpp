#include <gtest/gtest.h>
#include "containers/circular_buffer.hpp"
#include <numeric>
#include <algorithm>

using namespace trading;

TEST(CircularBufferTest, InitiallyEmpty) {
    CircularBuffer<int, 10> buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_FALSE(buf.full());
}

TEST(CircularBufferTest, PushAndAccess) {
    CircularBuffer<int, 10> buf;
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);

    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf[0], 1);
    EXPECT_EQ(buf[1], 2);
    EXPECT_EQ(buf[2], 3);
    EXPECT_EQ(buf.back(), 3);
    EXPECT_EQ(buf.front(), 1);
}

TEST(CircularBufferTest, OverflowWrap) {
    CircularBuffer<int, 4> buf;
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    buf.push_back(4);
    EXPECT_TRUE(buf.full());
    EXPECT_EQ(buf.size(), 4u);

    // Overflow: oldest (1) should be replaced
    buf.push_back(5);
    EXPECT_EQ(buf.size(), 4u);
    EXPECT_EQ(buf[0], 2); // Oldest is now 2
    EXPECT_EQ(buf[1], 3);
    EXPECT_EQ(buf[2], 4);
    EXPECT_EQ(buf[3], 5); // Newest
    EXPECT_EQ(buf.back(), 5);
    EXPECT_EQ(buf.front(), 2);
}

TEST(CircularBufferTest, MultipleWraps) {
    CircularBuffer<int, 3> buf;
    for (int i = 0; i < 100; ++i) {
        buf.push_back(i);
    }
    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf[0], 97);
    EXPECT_EQ(buf[1], 98);
    EXPECT_EQ(buf[2], 99);
    EXPECT_EQ(buf.back(), 99);
}

TEST(CircularBufferTest, Iterator) {
    CircularBuffer<int, 5> buf;
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i * 10);
    }

    std::vector<int> values;
    for (auto it = buf.begin(); it != buf.end(); ++it) {
        values.push_back(*it);
    }

    EXPECT_EQ(values.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(values[i], i * 10);
    }
}

TEST(CircularBufferTest, IteratorAfterWrap) {
    CircularBuffer<int, 3> buf;
    for (int i = 0; i < 5; ++i) {
        buf.push_back(i);
    }
    // Should contain 2, 3, 4
    std::vector<int> values(buf.begin(), buf.end());
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], 2);
    EXPECT_EQ(values[1], 3);
    EXPECT_EQ(values[2], 4);
}

TEST(CircularBufferTest, Capacity) {
    CircularBuffer<double, 100> buf;
    EXPECT_EQ(buf.capacity(), 100u);
}

TEST(CircularBufferTest, Clear) {
    CircularBuffer<int, 10> buf;
    buf.push_back(1);
    buf.push_back(2);
    EXPECT_EQ(buf.size(), 2u);

    buf.clear();
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
}

TEST(CircularBufferTest, DoubleValues) {
    CircularBuffer<double, 100> buf;
    for (int i = 0; i < 100; ++i) {
        buf.push_back(static_cast<double>(i) * 0.1);
    }
    EXPECT_TRUE(buf.full());
    EXPECT_NEAR(buf.back(), 9.9, 0.001);
}

TEST(CircularBufferTest, IteratorDifference) {
    CircularBuffer<int, 10> buf;
    for (int i = 0; i < 5; ++i) buf.push_back(i);
    auto dist = buf.end() - buf.begin();
    EXPECT_EQ(dist, 5);
}

TEST(CircularBufferTest, StdAccumulate) {
    CircularBuffer<int, 10> buf;
    for (int i = 1; i <= 5; ++i) buf.push_back(i);
    int sum = std::accumulate(buf.begin(), buf.end(), 0);
    EXPECT_EQ(sum, 15);
}
