#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <queue_c_api.h>

class QueueTest : public ::testing::Test {
protected:
  using local_allocator = local_buffer(16, 128);
  using list_allocator = growing_pool(8, 32, local_allocator);
  using queue_allocator = growing_pool(8, 32, local_allocator);

  local_allocator local_alloc;
  list_allocator list_alloc{&local_alloc};
  queue_allocator queue_alloc{&local_alloc};
};

TEST_F(QueueTest, CreateAndDestroy) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  ASSERT_NE(q, nullptr);
  ASSERT_TRUE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 0);
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, SingleEnqueueDequeue) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  enqueue_byte(q, 42);
  ASSERT_FALSE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 1);
  ASSERT_EQ(dequeue_byte(q), 42);
  ASSERT_TRUE(queue_is_empty(q));
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, FifoOrder) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  for (unsigned char i = 0; i < 10; i++) {
    enqueue_byte(q, i);
  }
  for (unsigned char i = 0; i < 10; i++) {
    ASSERT_EQ(dequeue_byte(q), i);
  }
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, MultipleQueues) {
  Q *q1 = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  Q *q2 = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  Q *q3 = create_queue(&local_alloc, &list_alloc, &queue_alloc);

  enqueue_byte(q1, 1);
  enqueue_byte(q2, 2);
  enqueue_byte(q3, 3);

  ASSERT_EQ(dequeue_byte(q1), 1);
  ASSERT_EQ(dequeue_byte(q2), 2);
  ASSERT_EQ(dequeue_byte(q3), 3);

  destroy_queue(q1, &queue_alloc);
  destroy_queue(q2, &queue_alloc);
  destroy_queue(q3, &queue_alloc);
}

TEST_F(QueueTest, BasicScenario) {
  // Expected output:
  // 0 1
  // 2 5
  // 3 4 6

  Q *q0 = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  enqueue_byte(q0, 0);
  enqueue_byte(q0, 1);

  Q *q1 = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  enqueue_byte(q1, 3);
  enqueue_byte(q0, 2);
  enqueue_byte(q1, 4);

  // Output: 0 1
  ASSERT_EQ(dequeue_byte(q0), 0);
  ASSERT_EQ(dequeue_byte(q0), 1);

  enqueue_byte(q0, 5);
  enqueue_byte(q1, 6);

  // Output: 2 5
  ASSERT_EQ(dequeue_byte(q0), 2);
  ASSERT_EQ(dequeue_byte(q0), 5);

  destroy_queue(q0, &queue_alloc);

  // Output: 3 4 6
  ASSERT_EQ(dequeue_byte(q1), 3);
  ASSERT_EQ(dequeue_byte(q1), 4);
  ASSERT_EQ(dequeue_byte(q1), 6);

  destroy_queue(q1, &queue_alloc);
}

TEST_F(QueueTest, DequeueFromEmptyQueue) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  EXPECT_DEATH(dequeue_byte(q), "");
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, DestroyNullQueue) {
  EXPECT_DEATH(destroy_queue(nullptr, &queue_alloc), "");
}

TEST_F(QueueTest, EnqueueToNullQueue) {
  EXPECT_DEATH(enqueue_byte(nullptr, 42), "");
}

TEST_F(QueueTest, DequeueFromNullQueue) {
  EXPECT_DEATH(dequeue_byte(nullptr), "");
}

TEST_F(QueueTest, QueueClear) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  for (int i = 0; i < 20; i++) {
    enqueue_byte(q, i);
  }
  ASSERT_EQ(queue_size(q), 20);
  queue_clear(q);
  ASSERT_TRUE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 0);
  destroy_queue(q, &queue_alloc);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(QueueTest, LargeQueue) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  // Fill a queue with many bytes (should trigger multiple ring buffer
  // allocations)
  for (int i = 0; i < 100; i++) {
    enqueue_byte(q, i % 256);
  }
  ASSERT_EQ(queue_size(q), 100);
  for (int i = 0; i < 100; i++) {
    ASSERT_EQ(dequeue_byte(q), i % 256);
  }
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, ManyQueues) {
  // Test creating many queues (up to 64 as per assignment)
  Q *queues[64];
  for (int i = 0; i < 30; i++) {
    queues[i] = create_queue(&local_alloc, &list_alloc, &queue_alloc);
    enqueue_byte(queues[i], i);
  }
  for (int i = 0; i < 30; i++) {
    ASSERT_EQ(dequeue_byte(queues[i]), i);
    destroy_queue(queues[i], &queue_alloc);
  }
}

TEST_F(QueueTest, InterleavedOperations) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  for (int i = 0; i < 50; i++) {
    enqueue_byte(q, i);
    enqueue_byte(q, i + 100);
    ASSERT_EQ(dequeue_byte(q), i);
  }
  // Should have 50 elements left
  ASSERT_EQ(queue_size(q), 50);
  for (int i = 0; i < 50; i++) {
    ASSERT_EQ(dequeue_byte(q), i + 100);
  }
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, AlternatingEnqueueDequeue) {
  Q *q = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  for (int iteration = 0; iteration < 100; iteration++) {
    enqueue_byte(q, iteration % 256);
    ASSERT_EQ(dequeue_byte(q), iteration % 256);
    ASSERT_TRUE(queue_is_empty(q));
  }
  destroy_queue(q, &queue_alloc);
}

TEST_F(QueueTest, TypicalUsagePattern) {
  // Simulate typical usage: 15 queues with ~80 bytes each
  Q *queues[15];

  for (int i = 0; i < 15; i++) {
    queues[i] = create_queue(&local_alloc, &list_alloc, &queue_alloc);
  }

  // Fill each queue with ~80 bytes
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 80; j++) {
      enqueue_byte(queues[i], (i * 80 + j) % 256);
    }
  }

  // Verify contents
  for (int i = 0; i < 15; i++) {
    ASSERT_EQ(queue_size(queues[i]), 80);
    for (int j = 0; j < 80; j++) {
      ASSERT_EQ(dequeue_byte(queues[i]), (i * 80 + j) % 256);
    }
  }

  for (int i = 0; i < 15; i++) {
    destroy_queue(queues[i], &queue_alloc);
  }
}
