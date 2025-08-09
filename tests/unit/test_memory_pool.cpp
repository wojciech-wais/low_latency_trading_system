#include <gtest/gtest.h>
#include "containers/memory_pool.hpp"
#include <set>

using namespace trading;

struct TestObj {
    uint64_t a;
    uint64_t b;
    double c;
};

TEST(MemoryPoolTest, AllocateAndDeallocate) {
    MemoryPool<TestObj, 100> pool;
    EXPECT_EQ(pool.allocated(), 0u);
    EXPECT_EQ(pool.available(), 100u);

    TestObj* ptr = pool.allocate();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(pool.allocated(), 1u);
    EXPECT_TRUE(pool.owns(ptr));

    pool.deallocate(ptr);
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(MemoryPoolTest, ExhaustPool) {
    constexpr size_t SIZE = 10;
    MemoryPool<TestObj, SIZE> pool;

    std::set<TestObj*> ptrs;
    for (size_t i = 0; i < SIZE; ++i) {
        TestObj* ptr = pool.allocate();
        ASSERT_NE(ptr, nullptr) << "Failed at allocation " << i;
        ptrs.insert(ptr);
    }
    EXPECT_EQ(pool.allocated(), SIZE);
    EXPECT_EQ(pool.available(), 0u);

    // Pool exhausted — next allocation should return nullptr
    EXPECT_EQ(pool.allocate(), nullptr);

    // All pointers should be unique
    EXPECT_EQ(ptrs.size(), SIZE);

    // Deallocate all
    for (TestObj* ptr : ptrs) {
        pool.deallocate(ptr);
    }
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(MemoryPoolTest, ReuseAfterFree) {
    MemoryPool<TestObj, 4> pool;

    TestObj* p1 = pool.allocate();
    pool.deallocate(p1);

    TestObj* p2 = pool.allocate();
    EXPECT_NE(p2, nullptr);
    // Should reuse the freed slot
    EXPECT_EQ(p1, p2);
    pool.deallocate(p2);
}

TEST(MemoryPoolTest, PointerRangeValidation) {
    MemoryPool<TestObj, 100> pool;

    TestObj* ptrs[100];
    for (int i = 0; i < 100; ++i) {
        ptrs[i] = pool.allocate();
        ASSERT_NE(ptrs[i], nullptr);
        EXPECT_TRUE(pool.owns(ptrs[i]));
    }

    // Stack-allocated object should not be owned
    TestObj stack_obj;
    EXPECT_FALSE(pool.owns(&stack_obj));

    for (int i = 0; i < 100; ++i) {
        pool.deallocate(ptrs[i]);
    }
}

TEST(MemoryPoolTest, WriteData) {
    MemoryPool<TestObj, 10> pool;
    TestObj* p = pool.allocate();
    ASSERT_NE(p, nullptr);

    p->a = 42;
    p->b = 99;
    p->c = 3.14;

    EXPECT_EQ(p->a, 42u);
    EXPECT_EQ(p->b, 99u);
    EXPECT_DOUBLE_EQ(p->c, 3.14);

    pool.deallocate(p);
}

TEST(MemoryPoolTest, DeallocateNull) {
    MemoryPool<TestObj, 10> pool;
    pool.deallocate(nullptr); // Should not crash
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(MemoryPoolTest, PoolSizeConstant) {
    MemoryPool<TestObj, 256> pool;
    EXPECT_EQ(pool.pool_size(), 256u);
}
