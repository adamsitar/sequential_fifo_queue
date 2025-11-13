#include "growing_pool.h"
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <memory>
#include <queue.h>

// Test configuration
constexpr size_t local_buffer_size = 16;
constexpr size_t local_buffer_count = 8; // 1KB total
constexpr size_t ring_buffer_capacity =
    4; // Small capacity to test multi-buffer behavior

using local_alloc = local_buffer(local_buffer_size, local_buffer_count);
using growing_pool_alloc = growing_pool(8, 32, local_alloc);
using test_queue =
    queue<int, ring_buffer_capacity, local_alloc, growing_pool_alloc>;

class QueueTest : public ::testing::Test {
protected:
  std::unique_ptr<local_alloc> local_allocator;
  std::unique_ptr<growing_pool_alloc> list_allocator;
  test_queue *q;

  void SetUp() override {
    local_allocator = std::make_unique<local_alloc>();
    list_allocator =
        std::make_unique<growing_pool_alloc>(local_allocator.get());
    q = new test_queue(local_allocator.get(), list_allocator.get());
  }

  void TearDown() override { delete q; }
};

// ============================================================================
// Basic Operations
// ============================================================================

TEST_F(QueueTest, InitiallyEmpty) {
  EXPECT_TRUE(q->empty());
  EXPECT_EQ(q->size(), 0);
}

TEST_F(QueueTest, PushAddsElement) {
  q->push(42);
  EXPECT_FALSE(q->empty());
  EXPECT_EQ(q->size(), 1);
}

TEST_F(QueueTest, PopRemovesElement) {
  q->push(42);
  int value = *q->pop();
  EXPECT_EQ(value, 42);
  EXPECT_TRUE(q->empty());
  EXPECT_EQ(q->size(), 0);
}

// ============================================================================
// FIFO Order
// ============================================================================

TEST_F(QueueTest, MaintainsFIFOOrder) {
  q->push(1);
  q->push(2);
  q->push(3);

  EXPECT_EQ(*q->pop(), 1);
  EXPECT_EQ(*q->pop(), 2);
  EXPECT_EQ(*q->pop(), 3);
  EXPECT_TRUE(q->empty());
}

// ============================================================================
// Multiple Ring Buffers
// ============================================================================

TEST_F(QueueTest, AllocatesMultipleRingBuffers) {
  // Push more than ring_buffer_capacity to force allocation of second
  // ring_buffer
  for (int i = 0; i < ring_buffer_capacity + 2; ++i) {
    q->push(i);
  }

  EXPECT_EQ(q->size(), ring_buffer_capacity + 2);

  // Verify FIFO order across ring_buffer boundaries
  for (int i = 0; i < ring_buffer_capacity + 2; ++i) {
    EXPECT_EQ(*q->pop(), i);
  }

  EXPECT_TRUE(q->empty());
}

TEST_F(QueueTest, DeallocatesEmptyRingBuffers) {
  // Fill first ring_buffer
  for (int i = 0; i < ring_buffer_capacity; ++i) {
    q->push(i);
  }

  // Add one more to create second ring_buffer
  q->push(100);

  EXPECT_EQ(q->size(), ring_buffer_capacity + 1);

  // Pop all from first ring_buffer (should deallocate it)
  for (int i = 0; i < ring_buffer_capacity; ++i) {
    EXPECT_EQ(*q->pop(), i);
  }

  // Last element should still be accessible
  EXPECT_EQ(q->size(), 1);
  EXPECT_EQ(*q->pop(), 100);
  EXPECT_TRUE(q->empty());
}

// ============================================================================
// Element Access
// ============================================================================

TEST_F(QueueTest, FrontReturnsOldestElement) {
  q->push(10);
  q->push(20);
  q->push(30);

  EXPECT_EQ(**q->front(), 10);
  EXPECT_EQ(q->size(), 3); // Front doesn't remove
}

TEST_F(QueueTest, BackReturnsNewestElement) {
  q->push(10);
  q->push(20);
  q->push(30);

  EXPECT_EQ(**q->back(), 30);
  EXPECT_EQ(q->size(), 3); // Back doesn't remove
}

TEST_F(QueueTest, FrontAndBackAcrossRingBuffers) {
  // Fill first ring_buffer
  for (int i = 0; i < ring_buffer_capacity; ++i) {
    q->push(i);
  }

  // Add to second ring_buffer
  q->push(100);
  q->push(200);

  EXPECT_EQ(**q->front(), 0);  // Oldest (in first ring_buffer)
  EXPECT_EQ(**q->back(), 200); // Newest (in second ring_buffer)
}

// ============================================================================
// Emplace
// ============================================================================

TEST_F(QueueTest, EmplaceConstructsInPlace) {
  q->emplace(42);
  EXPECT_EQ(q->size(), 1);
  EXPECT_EQ(**q->front(), 42);
}

// ============================================================================
// Clear
// ============================================================================

TEST_F(QueueTest, ClearEmptiesQueue) {
  // Add elements across multiple ring_buffers
  for (int i = 0; i < ring_buffer_capacity * 2; ++i) {
    q->push(i);
  }

  ASSERT_FALSE(q->empty());
  q->clear();

  EXPECT_TRUE(q->empty());
  EXPECT_EQ(q->size(), 0);
}

TEST_F(QueueTest, ClearEmptyQueue) {
  q->clear();
  EXPECT_TRUE(q->empty());
}

// ============================================================================
// Size Tracking
// ============================================================================

TEST_F(QueueTest, SizeTracksCorrectly) {
  EXPECT_EQ(q->size(), 0);

  q->push(1);
  EXPECT_EQ(q->size(), 1);

  q->push(2);
  EXPECT_EQ(q->size(), 2);

  q->pop();
  EXPECT_EQ(q->size(), 1);

  q->pop();
  EXPECT_EQ(q->size(), 0);
}

TEST_F(QueueTest, SizeAcrossMultipleRingBuffers) {
  // Add elements to create multiple ring_buffers
  for (int i = 0; i < ring_buffer_capacity * 3; ++i) {
    q->push(i);
    EXPECT_EQ(q->size(), i + 1);
  }

  // Remove elements
  for (int i = 0; i < ring_buffer_capacity * 3; ++i) {
    q->pop();
    EXPECT_EQ(q->size(), ring_buffer_capacity * 3 - i - 1);
  }
}

// ============================================================================
// Stress Test
// ============================================================================

TEST_F(QueueTest, PushPopCycle) {
  // Simulate queue usage with push/pop cycles
  for (int cycle = 0; cycle < 10; ++cycle) {
    // Fill queue
    for (int i = 0; i < ring_buffer_capacity * 2; ++i) {
      q->push(cycle * 100 + i);
    }

    // Drain half
    for (int i = 0; i < ring_buffer_capacity; ++i) {
      EXPECT_EQ(*q->pop(), cycle * 100 + i);
    }

    // Fill again
    for (int i = 0; i < ring_buffer_capacity; ++i) {
      q->push(static_cast<int>(cycle * 100 + ring_buffer_capacity * 2 + i));
    }

    // Drain completely
    for (int i = 0; i < ring_buffer_capacity * 2; ++i) {
      q->pop();
    }

    EXPECT_TRUE(q->empty());
  }
}
