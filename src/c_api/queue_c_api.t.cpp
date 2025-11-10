#include <gtest/gtest.h>
#include <queue_c_api.h>

TEST(BasicFunctionality, CreateAndDestroy) {
  Q *q = create_queue();
  ASSERT_NE(q, nullptr);
  ASSERT_TRUE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 0);
  destroy_queue(q);
}

TEST(BasicFunctionality, SingleEnqueueDequeue) {
  Q *q = create_queue();
  enqueue_byte(q, 42);
  ASSERT_FALSE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 1);
  ASSERT_EQ(dequeue_byte(q), 42);
  ASSERT_TRUE(queue_is_empty(q));
  destroy_queue(q);
}

TEST(BasicFunctionality, FifoOrder) {
  Q *q = create_queue();
  for (unsigned char i = 0; i < 10; i++) {
    enqueue_byte(q, i);
  }
  for (unsigned char i = 0; i < 10; i++) {
    ASSERT_EQ(dequeue_byte(q), i);
  }
  destroy_queue(q);
}

TEST(BasicFunctionality, MultipleQueues) {
  Q *q1 = create_queue();
  Q *q2 = create_queue();
  Q *q3 = create_queue();

  enqueue_byte(q1, 1);
  enqueue_byte(q2, 2);
  enqueue_byte(q3, 3);

  ASSERT_EQ(dequeue_byte(q1), 1);
  ASSERT_EQ(dequeue_byte(q2), 2);
  ASSERT_EQ(dequeue_byte(q3), 3);

  destroy_queue(q1);
  destroy_queue(q2);
  destroy_queue(q3);
}

TEST(AssignmentTest, BasicScenario) {
  // Expected output:
  // 0 1
  // 2 5
  // 3 4 6

  Q *q0 = create_queue();
  enqueue_byte(q0, 0);
  enqueue_byte(q0, 1);

  Q *q1 = create_queue();
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

  destroy_queue(q0);

  // Output: 3 4 6
  ASSERT_EQ(dequeue_byte(q1), 3);
  ASSERT_EQ(dequeue_byte(q1), 4);
  ASSERT_EQ(dequeue_byte(q1), 6);

  destroy_queue(q1);
}

TEST(EdgeCases, DequeueFromEmptyQueue) {
  Q *q = create_queue();
  EXPECT_DEATH(dequeue_byte(q), "");
  destroy_queue(q);
}

TEST(EdgeCases, DestroyNullQueue) { EXPECT_DEATH(destroy_queue(nullptr), ""); }

TEST(EdgeCases, EnqueueToNullQueue) {
  EXPECT_DEATH(enqueue_byte(nullptr, 42), "");
}

TEST(EdgeCases, DequeueFromNullQueue) {
  EXPECT_DEATH(dequeue_byte(nullptr), "");
}

TEST(EdgeCases, QueueClear) {
  Q *q = create_queue();
  for (int i = 0; i < 20; i++) {
    enqueue_byte(q, i);
  }
  ASSERT_EQ(queue_size(q), 20);
  queue_clear(q);
  ASSERT_TRUE(queue_is_empty(q));
  ASSERT_EQ(queue_size(q), 0);
  destroy_queue(q);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(StressTests, LargeQueue) {
  Q *q = create_queue();
  // Fill a queue with many bytes (should trigger multiple ring buffer
  // allocations)
  for (int i = 0; i < 100; i++) {
    enqueue_byte(q, i % 256);
  }
  ASSERT_EQ(queue_size(q), 100);
  for (int i = 0; i < 100; i++) {
    ASSERT_EQ(dequeue_byte(q), i % 256);
  }
  destroy_queue(q);
}

TEST(StressTests, ManyQueues) {
  // Test creating many queues (up to 64 as per assignment)
  Q *queues[64];
  for (int i = 0; i < 30; i++) {
    queues[i] = create_queue();
    enqueue_byte(queues[i], i);
  }
  for (int i = 0; i < 30; i++) {
    ASSERT_EQ(dequeue_byte(queues[i]), i);
    destroy_queue(queues[i]);
  }
}

TEST(StressTests, InterleavedOperations) {
  Q *q = create_queue();
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
  destroy_queue(q);
}

TEST(StressTests, AlternatingEnqueueDequeue) {
  Q *q = create_queue();
  for (int iteration = 0; iteration < 100; iteration++) {
    enqueue_byte(q, iteration % 256);
    ASSERT_EQ(dequeue_byte(q), iteration % 256);
    ASSERT_TRUE(queue_is_empty(q));
  }
  destroy_queue(q);
}

TEST(StressTests, TypicalUsagePattern) {
  // Simulate typical usage: 15 queues with ~80 bytes each
  Q *queues[15];

  for (int i = 0; i < 15; i++) {
    queues[i] = create_queue();
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
    destroy_queue(queues[i]);
  }
}
