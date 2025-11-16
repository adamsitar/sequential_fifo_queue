#include <gtest/gtest.h>
#include <iterators/iterator_test_suite.h>
#include <local_buffer.h>
#include <ring_buffer.h>
#include <vector>

// ============================================================================
// Generic Iterator Test Adapter for ring_buffer
// ============================================================================

struct ring_buffer_adapter {
  using value_type = int;

  static constexpr size_t allocator_block_size = 128;
  static constexpr size_t allocator_block_count = 4;
  static constexpr size_t capacity = 16; // Larger capacity for testing

  using allocator_type = local_buffer(allocator_block_size,
                                      allocator_block_count);
  using container_type = ring_buffer<value_type, capacity, allocator_type>;

  static void setup_allocator(allocator_type &alloc) { (void)alloc; }
  static void cleanup_allocator(allocator_type &alloc) { (void)alloc; }

  static void initialize_container(container_type &container,
                                   allocator_type &alloc) {
    new (&container) container_type(&alloc);
  }

  static void cleanup_container(container_type &container) {
    container.~container_type();
  }

  static void populate(container_type &container,
                       std::vector<value_type> const &values) {
    for (auto const &value : values) {
      container.push(value);
    }
  }
};

// Instantiate generic random access iterator tests for ring_buffer
using RingBufferIteratorTypes = ::testing::Types<ring_buffer_adapter>;
INSTANTIATE_TYPED_TEST_SUITE_P(RingBuffer, GenericRandomAccessIteratorTestSuite,
                               RingBufferIteratorTypes);

// ============================================================================
// ring_buffer Specific Tests
// ============================================================================

constexpr size_t allocator_block_size = 128;
constexpr size_t allocator_block_count = 4;
constexpr size_t ring_buffer_capacity = 8;

using test_allocator = local_buffer(allocator_block_size,
                                    allocator_block_count);
using test_ring_buffer = ring_buffer<int, ring_buffer_capacity, test_allocator>;

class RingBufferTest : public ::testing::Test {
protected:
  test_allocator allocator;
  test_ring_buffer *buffer;

  void SetUp() override { buffer = new test_ring_buffer(&allocator); }

  void TearDown() override { delete buffer; }
};

// ============================================================================
// Basic Operations
// ============================================================================

TEST_F(RingBufferTest, InitiallyEmpty) {
  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(buffer->size(), 0);
  EXPECT_EQ(buffer->capacity(), ring_buffer_capacity);
  EXPECT_EQ(buffer->get_free(), ring_buffer_capacity);
}

TEST_F(RingBufferTest, PushAddsElement) {
  buffer->push(42);
  EXPECT_FALSE(buffer->empty());
  EXPECT_EQ(buffer->size(), 1);
  EXPECT_EQ(buffer->get_free(), ring_buffer_capacity - 1);
}

TEST_F(RingBufferTest, PopRemovesElement) {
  buffer->push(42);
  int value = *buffer->pop();
  EXPECT_EQ(value, 42);
  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(buffer->size(), 0);
}

TEST_F(RingBufferTest, FrontReturnsOldestElement) {
  buffer->push(10);
  buffer->push(20);
  buffer->push(30);

  EXPECT_EQ(buffer->front(), 10);
  EXPECT_EQ(buffer->size(), 3); // Front doesn't remove
}

TEST_F(RingBufferTest, BackReturnsNewestElement) {
  buffer->push(10);
  buffer->push(20);
  buffer->push(30);

  EXPECT_EQ(buffer->back(), 30);
  EXPECT_EQ(buffer->size(), 3); // Back doesn't remove
}

// ============================================================================
// FIFO Order
// ============================================================================

TEST_F(RingBufferTest, MaintainsFIFOOrder) {
  buffer->push(1);
  buffer->push(2);
  buffer->push(3);

  EXPECT_EQ(*buffer->pop(), 1);
  EXPECT_EQ(*buffer->pop(), 2);
  EXPECT_EQ(*buffer->pop(), 3);
  EXPECT_TRUE(buffer->empty());
}

// ============================================================================
// Capacity & Full
// ============================================================================

TEST_F(RingBufferTest, FillToCapacity) {
  for (size_t i = 0; i < ring_buffer_capacity; ++i) {
    ASSERT_FALSE(buffer->is_full());
    buffer->push(static_cast<int>(i));
  }

  EXPECT_TRUE(buffer->is_full());
  EXPECT_EQ(buffer->size(), ring_buffer_capacity);
  EXPECT_EQ(buffer->get_free(), 0);
}

TEST_F(RingBufferTest, EmptyAfterFillingAndDraining) {
  // Fill
  for (size_t i = 0; i < ring_buffer_capacity; ++i) {
    buffer->push(static_cast<int>(i));
  }

  // Drain
  for (size_t i = 0; i < ring_buffer_capacity; ++i) {
    buffer->pop();
  }

  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(buffer->size(), 0);
}

// ============================================================================
// Wrap-Around (Circular Behavior)
// ============================================================================

TEST_F(RingBufferTest, WrapsAroundCorrectly) {
  // Fill buffer
  for (size_t i = 0; i < ring_buffer_capacity; ++i) {
    buffer->push(static_cast<int>(i));
  }

  // Pop half
  for (size_t i = 0; i < ring_buffer_capacity / 2; ++i) {
    EXPECT_EQ(*buffer->pop(), static_cast<int>(i));
  }

  // Push new elements (should wrap around)
  for (size_t i = 0; i < ring_buffer_capacity / 2; ++i) {
    buffer->push(100 + static_cast<int>(i));
  }

  EXPECT_TRUE(buffer->is_full());

  // Verify old elements still there
  for (size_t i = ring_buffer_capacity / 2; i < ring_buffer_capacity; ++i) {
    EXPECT_EQ(*buffer->pop(), static_cast<int>(i));
  }

  // Verify new wrapped elements
  for (size_t i = 0; i < ring_buffer_capacity / 2; ++i) {
    EXPECT_EQ(*buffer->pop(), 100 + static_cast<int>(i));
  }

  EXPECT_TRUE(buffer->empty());
}

// ============================================================================
// Clear
// ============================================================================

TEST_F(RingBufferTest, ClearEmptiesBuffer) {
  buffer->push(1);
  buffer->push(2);
  buffer->push(3);

  ASSERT_FALSE(buffer->empty());
  buffer->clear();

  EXPECT_TRUE(buffer->empty());
  EXPECT_EQ(buffer->size(), 0);
  EXPECT_EQ(buffer->get_free(), ring_buffer_capacity);
}

// ============================================================================
// Emplace
// ============================================================================

TEST_F(RingBufferTest, EmplaceConstructsInPlace) {
  int &ref = buffer->emplace(42);
  EXPECT_EQ(ref, 42);
  EXPECT_EQ(buffer->size(), 1);
  EXPECT_EQ(buffer->front(), 42);
}

// ============================================================================
// Element Access via Index
// ============================================================================

TEST_F(RingBufferTest, IndexAccess) {
  buffer->push(10);
  buffer->push(20);
  buffer->push(30);

  EXPECT_EQ((*buffer)[0], 10); // Oldest
  EXPECT_EQ((*buffer)[1], 20);
  EXPECT_EQ((*buffer)[2], 30); // Newest
}

TEST_F(RingBufferTest, IndexAccessWithWrapAround) {
  // Fill buffer
  for (size_t i = 0; i < ring_buffer_capacity; ++i) {
    buffer->push(static_cast<int>(i));
  }

  // Pop some, push some (causes wrap)
  buffer->pop();
  buffer->pop();
  buffer->push(100);
  buffer->push(101);

  // Index 0 should be the oldest element (which is now 2)
  EXPECT_EQ((*buffer)[0], 2);
  // Last index should be newest
  EXPECT_EQ((*buffer)[buffer->size() - 1], 101);
}

// ============================================================================
// Move Semantics
// ============================================================================

TEST_F(RingBufferTest, MovesElementsCorrectly) {
  std::string str = "test";
  buffer->push(std::move(42)); // Move rvalue
  EXPECT_EQ(buffer->front(), 42);
}
