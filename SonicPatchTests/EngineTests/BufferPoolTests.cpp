//
// BufferPoolTests.cpp
//
#include "Core/BufferPool.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>

using namespace sonicpatch;

TEST(BufferPoolTests, ReserveAllocatesAllSizeClasses) {
    BufferPool pool;
    ASSERT_TRUE(pool.reserve(/*countPerSize=*/4));

    // Size classes are 64,128,256,512,1024,2048,4096 -> 7 classes.
    // Each over-provisioned 2x: 4 * 2 = 8 buffers per class -> 56 total.
    EXPECT_EQ(pool.totalBuffers(), static_cast<size_t>(7 * 8));
}

TEST(BufferPoolTests, AcquireGivesDistinctAlignedBuffers) {
    BufferPool pool;
    ASSERT_TRUE(pool.reserve(4));

    std::set<float*> seen;
    std::vector<size_t> handles;
    for (int i = 0; i < 8; ++i) {
        size_t idx = 0;
        PooledBuffer* b = pool.acquire(512, idx);
        ASSERT_NE(b, nullptr);
        EXPECT_GE(b->capacity, 512u);
        // Distinct pointers.
        EXPECT_TRUE(seen.insert(b->data).second);
        // Aligned to kAlignment.
        EXPECT_EQ(reinterpret_cast<uintptr_t>(b->data) % BufferPool::kAlignment, 0u);
        handles.push_back(idx);
    }

    // Release one and re-acquire: should succeed again.
    pool.release(handles.front());
    size_t idx = 0;
    EXPECT_NE(pool.acquire(512, idx), nullptr);
}

TEST(BufferPoolTests, AcquireRoundsUpToSizeClass) {
    BufferPool pool;
    ASSERT_TRUE(pool.reserve(2));

    size_t idx = 0;
    PooledBuffer* b = pool.acquire(100, idx); // -> 128 class
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->capacity, 128u);
}

TEST(BufferPoolTests, ExhaustingSizeClassReturnsNull) {
    BufferPool pool;
    ASSERT_TRUE(pool.reserve(1)); // 2 buffers per class

    size_t a = 0, b = 0, c = 0;
    EXPECT_NE(pool.acquire(64, a), nullptr);
    EXPECT_NE(pool.acquire(64, b), nullptr);
    EXPECT_EQ(pool.acquire(64, c), nullptr); // exhausted this class
}
