#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <memory>
#include <queue.h>
#include <vector>

constexpr size_t MAX_QUEUES = 64;
constexpr size_t AVERAGE_QUEUES = 15;
constexpr size_t AVERAGE_BYTES_PER_QUEUE = 80;
constexpr size_t RING_BUFFER_CAPACITY = 16;

// Base allocator - 2KB fixed memory pool
using local_alloc = local_buffer(16, 128);
// Pool for list nodes (offset_list nodes)
using list_node_pool = growing_pool(8, 32, local_alloc);
// Pool for queue objects themselves
using queue_object_pool = growing_pool(4, 64, local_alloc);
// The queue type
using byte_queue =
    queue<unsigned char, RING_BUFFER_CAPACITY, local_alloc, list_node_pool>;

// Verify queue fits in allocator block
static_assert(sizeof(byte_queue) <= queue_object_pool::block_size,
              "Queue size exceeds allocator block size");

// Test fixture that manages shared allocators for multiple queues
class QueueAssignmentTest : public ::testing::Test {
protected:
  std::unique_ptr<local_alloc> local_allocator;
  std::unique_ptr<list_node_pool> list_allocator;
  std::unique_ptr<queue_object_pool> queue_allocator;
  std::vector<byte_queue *> queues;

  void SetUp() override {
    // Create shared allocators - all share the same 2KB memory pool
    local_allocator = std::make_unique<local_alloc>();

    // Set OOM callback - terminates when memory exhausted
    local_allocator->set_oom_callback([]() {
      std::abort();  // Assignment spec: on_out_of_memory() does not return
    });

    list_allocator = std::make_unique<list_node_pool>(local_allocator.get());
    queue_allocator =
        std::make_unique<queue_object_pool>(local_allocator.get());
  }

  void TearDown() override {
    // Destroy all queues - must call destructor and deallocate
    for (auto *q : queues) {
      q->~byte_queue();
      typename queue_object_pool::pointer_type ptr{static_cast<void *>(q)};
      queue_allocator->deallocate_block(ptr);
    }
    queues.clear();
  }

  // Create a new queue (analogous to create_queue())
  // Allocates queue object from the buffer, not from heap
  byte_queue *create_queue() {
    // Verify the block size is large enough for the queue object
    constexpr size_t required_size = sizeof(byte_queue);
    constexpr size_t block_size = queue_object_pool::block_size;
    fatal(required_size > block_size,
          "Queue object size exceeds allocator block size");

    // Allocate memory for queue object from the pool
    auto ptr_result = queue_allocator->allocate_block();
    if (!ptr_result.has_value()) {
      return nullptr; // Out of memory
    }

    void *mem = static_cast<void *>(ptr_result.value());

    // Construct queue object in-place using placement new
    // Ring buffers use local_allocator directly, lists use list_allocator
    byte_queue *q =
        new (mem) byte_queue(local_allocator.get(), list_allocator.get());
    queues.push_back(q);
    return q;
  }

  // Destroy a queue (analogous to destroy_queue())
  void destroy_queue(byte_queue *q) {
    auto it = std::find(queues.begin(), queues.end(), q);
    if (it != queues.end()) {
      // Explicitly call destructor
      (*it)->~byte_queue();

      // Deallocate the memory back to the pool
      typename queue_object_pool::pointer_type ptr{static_cast<void *>(*it)};
      queue_allocator->deallocate_block(ptr);

      queues.erase(it);
    }
  }

  // Enqueue a byte (analogous to enqueue_byte())
  void enqueue_byte(byte_queue *q, unsigned char b) {
    auto result = q->push(b);
    ASSERT_TRUE(result.has_value()) << "Failed to enqueue byte";
  }

  // Dequeue a byte (analogous to dequeue_byte())
  unsigned char dequeue_byte(byte_queue *q) {
    auto result = q->pop();
    if (!result.has_value()) {
      ADD_FAILURE() << "Failed to dequeue byte";
      return 0;
    }
    return result.value();
  }
};

// ============================================================================
// Assignment Example Test
// ============================================================================

TEST_F(QueueAssignmentTest, ExactAssignmentExample) {
  // This test replicates the exact example from the assignment
  // Expected output: 0 1, 2 5, 3 4 6

  byte_queue *q0 = create_queue();
  enqueue_byte(q0, 0);
  enqueue_byte(q0, 1);

  byte_queue *q1 = create_queue();
  enqueue_byte(q1, 3);
  enqueue_byte(q0, 2);
  enqueue_byte(q1, 4);

  // First line: 0 1
  EXPECT_EQ(dequeue_byte(q0), 0);
  EXPECT_EQ(dequeue_byte(q0), 1);

  enqueue_byte(q0, 5);
  enqueue_byte(q1, 6);

  // Second line: 2 5
  EXPECT_EQ(dequeue_byte(q0), 2);
  EXPECT_EQ(dequeue_byte(q0), 5);

  destroy_queue(q0);

  // Third line: 3 4 6
  EXPECT_EQ(dequeue_byte(q1), 3);
  EXPECT_EQ(dequeue_byte(q1), 4);
  EXPECT_EQ(dequeue_byte(q1), 6);

  destroy_queue(q1);

  EXPECT_TRUE(queues.empty());
}

// ============================================================================
// Multiple Queues Tests
// ============================================================================

TEST_F(QueueAssignmentTest, MultipleQueuesIsolation) {
  // Test that operations on one queue don't affect others
  constexpr int NUM_QUEUES = 5;
  byte_queue *qs[NUM_QUEUES];

  // Create queues and add unique data to each
  for (int i = 0; i < NUM_QUEUES; ++i) {
    qs[i] = create_queue();
    for (int j = 0; j < 10; ++j) {
      enqueue_byte(qs[i], static_cast<unsigned char>(i * 100 + j));
    }
  }

  // Verify each queue has correct independent data
  for (int i = 0; i < NUM_QUEUES; ++i) {
    EXPECT_EQ(qs[i]->size(), 10);
    for (int j = 0; j < 10; ++j) {
      EXPECT_EQ(dequeue_byte(qs[i]), static_cast<unsigned char>(i * 100 + j));
    }
    EXPECT_TRUE(qs[i]->empty());
  }

  // Cleanup
  for (int i = 0; i < NUM_QUEUES; ++i) {
    destroy_queue(qs[i]);
  }
}

TEST_F(QueueAssignmentTest, MaximumQueues) {
  // Test creating up to 64 queues (assignment requirement)
  // With minimal data to stay within 2KB

  std::vector<byte_queue *> max_queues;

  // Create MAX_QUEUES queues
  for (size_t i = 0; i < MAX_QUEUES; ++i) {
    byte_queue *q = create_queue();
    ASSERT_NE(q, nullptr);
    max_queues.push_back(q);

    // Add at least one byte to each queue to verify it works
    enqueue_byte(q, static_cast<unsigned char>(i));
  }

  EXPECT_EQ(max_queues.size(), MAX_QUEUES);

  // Verify each queue has its data
  for (size_t i = 0; i < MAX_QUEUES; ++i) {
    EXPECT_EQ(max_queues[i]->size(), 1);
    EXPECT_EQ(dequeue_byte(max_queues[i]), static_cast<unsigned char>(i));
  }

  // Cleanup
  for (auto *q : max_queues) {
    destroy_queue(q);
  }
}

TEST_F(QueueAssignmentTest, AverageCase) {
  // Test average case: 15 queues with ~80 bytes each
  // Total: 15 * 80 = 1200 bytes (well within 2KB limit)

  std::vector<byte_queue *> avg_queues;

  for (size_t i = 0; i < AVERAGE_QUEUES; ++i) {
    byte_queue *q = create_queue();
    avg_queues.push_back(q);

    // Add approximately 80 bytes to each queue
    for (size_t j = 0; j < AVERAGE_BYTES_PER_QUEUE; ++j) {
      enqueue_byte(q, static_cast<unsigned char>((i + j) % 256));
    }
  }

  EXPECT_EQ(avg_queues.size(), AVERAGE_QUEUES);

  // Verify each queue has correct size and FIFO order
  for (size_t i = 0; i < AVERAGE_QUEUES; ++i) {
    EXPECT_EQ(avg_queues[i]->size(), AVERAGE_BYTES_PER_QUEUE);

    // Dequeue and verify FIFO order
    for (size_t j = 0; j < AVERAGE_BYTES_PER_QUEUE; ++j) {
      EXPECT_EQ(dequeue_byte(avg_queues[i]),
                static_cast<unsigned char>((i + j) % 256));
    }

    EXPECT_TRUE(avg_queues[i]->empty());
  }

  // Cleanup
  for (auto *q : avg_queues) {
    destroy_queue(q);
  }
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(QueueAssignmentTest, DequeueFromEmptyQueue) {
  // Test illegal operation: dequeue from empty queue
  byte_queue *q = create_queue();

  EXPECT_TRUE(q->empty());

  // Attempting to dequeue from empty should fail
  auto result = q->pop();
  EXPECT_FALSE(result.has_value());

  destroy_queue(q);
}

TEST_F(QueueAssignmentTest, SingleQueueCanUseMostMemory) {
  // Test that a single queue can use most of the available memory
  // This verifies no artificial bounds on individual queue size

  byte_queue *q = create_queue();

  // Fill a substantial portion of the 2KB (conservative to avoid OOM)
  // Account for metadata overhead: queue objects, list nodes, ring buffers
  constexpr size_t SAFE_SIZE = 1000;

  for (size_t i = 0; i < SAFE_SIZE; ++i) {
    auto result = q->push(static_cast<unsigned char>(i % 256));
    ASSERT_TRUE(result.has_value())
        << "Should be able to store at least " << SAFE_SIZE << " bytes, failed at " << i;
  }

  // Verify FIFO order
  EXPECT_EQ(q->size(), SAFE_SIZE);
  for (size_t i = 0; i < SAFE_SIZE; ++i) {
    EXPECT_EQ(dequeue_byte(q), static_cast<unsigned char>(i % 256));
  }

  EXPECT_TRUE(q->empty());
  destroy_queue(q);
}

TEST_F(QueueAssignmentTest, OutOfMemoryTriggersCallback) {
  // Test that exceeding 2KB triggers OOM callback (which aborts)
  EXPECT_DEATH({
    byte_queue *q = create_queue();

    // Try to allocate way more than 2KB
    for (size_t i = 0; i < 10000; ++i) {
      q->push(static_cast<unsigned char>(i % 256));
    }
  }, ".*");  // Expect termination (abort)
}

// ============================================================================
// Dynamic Behavior Tests
// ============================================================================

TEST_F(QueueAssignmentTest, InterleavedOperations) {
  // Test interleaved create, enqueue, dequeue, destroy operations

  byte_queue *q1 = create_queue();
  enqueue_byte(q1, 10);

  byte_queue *q2 = create_queue();
  enqueue_byte(q2, 20);
  enqueue_byte(q1, 11);

  EXPECT_EQ(dequeue_byte(q1), 10);

  byte_queue *q3 = create_queue();
  enqueue_byte(q3, 30);

  EXPECT_EQ(dequeue_byte(q2), 20);
  EXPECT_EQ(dequeue_byte(q1), 11);

  destroy_queue(q1);

  enqueue_byte(q2, 21);
  enqueue_byte(q3, 31);

  EXPECT_EQ(dequeue_byte(q2), 21);
  EXPECT_EQ(dequeue_byte(q3), 30);
  EXPECT_EQ(dequeue_byte(q3), 31);

  destroy_queue(q2);
  destroy_queue(q3);

  EXPECT_TRUE(queues.empty());
}

TEST_F(QueueAssignmentTest, QueueCreationAndDestructionCycles) {
  // Stress test: repeated cycles of creating and destroying queues
  // This tests memory reuse and fragmentation handling

  constexpr int CYCLES = 10;
  constexpr int QUEUES_PER_CYCLE = 10;
  constexpr int BYTES_PER_QUEUE = 20;

  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    std::vector<byte_queue *> cycle_queues;

    // Create queues and fill with data
    for (int i = 0; i < QUEUES_PER_CYCLE; ++i) {
      byte_queue *q = create_queue();
      cycle_queues.push_back(q);

      for (int j = 0; j < BYTES_PER_QUEUE; ++j) {
        enqueue_byte(q, static_cast<unsigned char>(cycle * 100 + i * 10 + j));
      }
    }

    // Verify and drain queues
    for (int i = 0; i < QUEUES_PER_CYCLE; ++i) {
      for (int j = 0; j < BYTES_PER_QUEUE; ++j) {
        EXPECT_EQ(dequeue_byte(cycle_queues[i]),
                  static_cast<unsigned char>(cycle * 100 + i * 10 + j));
      }
      EXPECT_TRUE(cycle_queues[i]->empty());
    }

    // Destroy all queues from this cycle
    for (auto *q : cycle_queues) {
      destroy_queue(q);
    }

    EXPECT_TRUE(queues.empty());
  }
}

TEST_F(QueueAssignmentTest, VariableSizeQueues) {
  // Test mix of small and large queues

  byte_queue *small1 = create_queue();
  byte_queue *large = create_queue();
  byte_queue *small2 = create_queue();

  // Small queues with 5 bytes each
  for (int i = 0; i < 5; ++i) {
    enqueue_byte(small1, static_cast<unsigned char>(10 + i));
    enqueue_byte(small2, static_cast<unsigned char>(20 + i));
  }

  // Large queue with 200 bytes
  for (int i = 0; i < 200; ++i) {
    enqueue_byte(large, static_cast<unsigned char>(i));
  }

  // Verify sizes
  EXPECT_EQ(small1->size(), 5);
  EXPECT_EQ(small2->size(), 5);
  EXPECT_EQ(large->size(), 200);

  // Verify FIFO for small queues
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(dequeue_byte(small1), static_cast<unsigned char>(10 + i));
  }

  // Verify FIFO for large queue
  for (int i = 0; i < 200; ++i) {
    EXPECT_EQ(dequeue_byte(large), static_cast<unsigned char>(i));
  }

  // Verify second small queue
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(dequeue_byte(small2), static_cast<unsigned char>(20 + i));
  }

  destroy_queue(small1);
  destroy_queue(small2);
  destroy_queue(large);
}

TEST_F(QueueAssignmentTest, FrontAndBackAccessMultipleQueues) {
  // Test front/back access across multiple queues

  byte_queue *q1 = create_queue();
  byte_queue *q2 = create_queue();

  enqueue_byte(q1, 100);
  enqueue_byte(q1, 101);
  enqueue_byte(q1, 102);

  enqueue_byte(q2, 200);
  enqueue_byte(q2, 201);

  // Verify front and back without removing
  auto q1_front = q1->front();
  auto q1_back = q1->back();
  ASSERT_TRUE(q1_front.has_value());
  ASSERT_TRUE(q1_back.has_value());
  EXPECT_EQ(*q1_front.value(), 100);
  EXPECT_EQ(*q1_back.value(), 102);

  auto q2_front = q2->front();
  auto q2_back = q2->back();
  ASSERT_TRUE(q2_front.has_value());
  ASSERT_TRUE(q2_back.has_value());
  EXPECT_EQ(*q2_front.value(), 200);
  EXPECT_EQ(*q2_back.value(), 201);

  // Sizes should be unchanged
  EXPECT_EQ(q1->size(), 3);
  EXPECT_EQ(q2->size(), 2);

  destroy_queue(q1);
  destroy_queue(q2);
}

TEST_F(QueueAssignmentTest, EmptyQueueAfterClear) {
  byte_queue *q = create_queue();

  for (int i = 0; i < 50; ++i) {
    enqueue_byte(q, static_cast<unsigned char>(i));
  }

  EXPECT_EQ(q->size(), 50);
  EXPECT_FALSE(q->empty());

  q->clear();

  EXPECT_EQ(q->size(), 0);
  EXPECT_TRUE(q->empty());

  // Should be able to reuse after clear
  enqueue_byte(q, 99);
  EXPECT_EQ(dequeue_byte(q), 99);

  destroy_queue(q);
}
