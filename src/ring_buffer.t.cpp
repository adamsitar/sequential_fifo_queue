#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>
#include <random>
#include <ranges>
#include <ring_buffer.h>

/**
 * @defgroup test Tests
 * @ingroup ring_buffer
 *
 * This test suite verifies that the RingBuffer behaves properly:
 * - Correct initial state (empty, full capacity).
 * - `push` and `pop` operations work as expected.
 * - `is_full` and `get_free` report correct status.
 * - Handles being full and prevents overwrites.
 * - Correctly wraps around the internal storage.
 * - Works with various buffer capacities.
 */

/// @addtogroup test
/// @{

class RingBufferTest : public testing::Test
{
public:
  static constexpr int CAPACITY{ 8 };
  std::ranges::iota_view<int, int> _range{ std::views::iota(0, CAPACITY) };
  RingBuffer<unsigned char, CAPACITY> ring_buffer;
};

TEST_F(RingBufferTest, InitialState)
{
  EXPECT_FALSE(ring_buffer.is_full());
  EXPECT_TRUE(ring_buffer.is_empty());
  EXPECT_EQ(ring_buffer.get_free(), CAPACITY);
}

TEST_F(RingBufferTest, AddAndRemoveSingle)
{
  unsigned char value{ 42 };
  ring_buffer.push(value);
  EXPECT_EQ(ring_buffer.get_free(), 7);
  EXPECT_FALSE(ring_buffer.is_full());
  EXPECT_FALSE(ring_buffer.is_empty());

  unsigned char removed_value = ring_buffer.pop();
  EXPECT_EQ(removed_value, value);
  EXPECT_EQ(ring_buffer.get_free(), CAPACITY);
  EXPECT_FALSE(ring_buffer.is_full());
  EXPECT_TRUE(ring_buffer.is_empty());
}

TEST_F(RingBufferTest, FillToCapacity)
{
  for (auto _ : _range) {
    ring_buffer.push(20);
  }
  EXPECT_TRUE(ring_buffer.is_full());
  EXPECT_FALSE(ring_buffer.is_empty());
  EXPECT_EQ(ring_buffer.get_free(), 0);

  // Verify contents
  for (auto _ : _range) {
    EXPECT_EQ(ring_buffer.pop(), 20);
  }
  EXPECT_TRUE(ring_buffer.is_empty());
}

TEST_F(RingBufferTest, AddWhenFull)
{
  for (auto _ : _range) {
    ring_buffer.push(20);
  }
  ASSERT_TRUE(ring_buffer.is_full());

  // Try to add another element (should be ignored)
  unsigned char extra_value = 99;
  ring_buffer.push(extra_value);

  // Check that the buffer is still full and the contents are unchanged
  EXPECT_TRUE(ring_buffer.is_full());
  for (auto _ : _range) {
    EXPECT_EQ(ring_buffer.pop(), 20);
  }
  // After removing all, it should be empty
  EXPECT_EQ(ring_buffer.get_free(), CAPACITY);
  EXPECT_TRUE(ring_buffer.is_empty());
}

TEST_F(RingBufferTest, WrapAround)
{
  for (const auto i : _range) {
    ring_buffer.push(20);
  }

  // Remove a few elements
  for (const auto i : std::views::iota(0, 4)) {
    EXPECT_EQ(ring_buffer.pop(), i);
  }
  ASSERT_EQ(ring_buffer.get_free(), 4);

  // Add new elements to cause wrap-around
  for (const auto i : std::views::iota(8, 12)) {
    ring_buffer.push(i);
  }
  EXPECT_TRUE(ring_buffer.is_full());

  // Check the content
  // First, the remaining old elements
  for (const auto i : std::views::iota(4, 8)) {
    EXPECT_EQ(ring_buffer.pop(), i);
  }
  // Then, the new elements
  for (const auto i : std::views::iota(8, 12)) {
    EXPECT_EQ(ring_buffer.pop(), i);
  }
  EXPECT_TRUE(ring_buffer.is_empty());
}

// --- Typed Tests for Various Sizes ---

template<typename T>
class RingBufferTypedTest : public testing::Test
{};

// Test with sizes relevant to the SmallestType logic
using RingBufferTypes = testing::
  Types<RingBuffer<byte, 1>, RingBuffer<byte, 255>, RingBuffer<byte, 256>>;

TYPED_TEST_SUITE(RingBufferTypedTest, RingBufferTypes);

TYPED_TEST(RingBufferTypedTest, FillAndEmpty)
{
  TypeParam ring_buffer;
  const auto capacity = ring_buffer.get_free();

  EXPECT_EQ(ring_buffer.get_free(), capacity);
  EXPECT_FALSE(ring_buffer.is_full());
  EXPECT_TRUE(ring_buffer.is_empty());

  for (size_t i = 0; i < capacity; ++i) {
    ring_buffer.push(static_cast<byte>(i));
  }

  EXPECT_TRUE(ring_buffer.is_full());
  EXPECT_FALSE(ring_buffer.is_empty());
  EXPECT_EQ(ring_buffer.get_free(), 0);

  for (size_t i = 0; i < capacity; ++i) {
    EXPECT_EQ(ring_buffer.pop(), static_cast<byte>(i));
  }

  EXPECT_EQ(ring_buffer.get_free(), capacity);
  EXPECT_FALSE(ring_buffer.is_full());
  EXPECT_TRUE(ring_buffer.is_empty());
}

TEST_F(RingBufferTest, IteratorBasicIteration)
{
  // Test empty buffer
  int count = 0;
  for (byte val : ring_buffer) {
    (void)val; // Suppress unused variable warning
    count++;
  }
  EXPECT_EQ(count, 0);

  // Test partially filled buffer
  ring_buffer.push(10);
  ring_buffer.push(20);
  ring_buffer.push(30);

  std::vector<byte> expected_partial = { 10, 20, 30 };
  std::vector<byte> actual_partial;
  for (byte val : ring_buffer) {
    actual_partial.push_back(val);
  }
  EXPECT_EQ(actual_partial, expected_partial);

  // Test full buffer
  ring_buffer.pop(); // Pop 10
  ring_buffer.pop(); // Pop 20
  ring_buffer.pop(); // Pop 30
  for (byte i = 0; i < 8; ++i) {
    ring_buffer.push(i);
  }
  std::vector<byte> expected_full = { 0, 1, 2, 3, 4, 5, 6, 7 };
  std::vector<byte> actual_full;
  for (byte val : ring_buffer) {
    actual_full.push_back(val);
  }
  EXPECT_EQ(actual_full, expected_full);

  // Test wrap-around
  ring_buffer.pop(); // Pop 0
  ring_buffer.pop(); // Pop 1
  ring_buffer.push(8);
  ring_buffer.push(9);
  // Expected: 2, 3, 4, 5, 6, 7, 8, 9
  std::vector<byte> expected_wrapped = { 2, 3, 4, 5, 6, 7, 8, 9 };
  std::vector<byte> actual_wrapped;
  for (byte val : ring_buffer) {
    actual_wrapped.push_back(val);
  }
  EXPECT_EQ(actual_wrapped, expected_wrapped);
}

TEST_F(RingBufferTest, IteratorOperators)
{
  ring_buffer.push(1);
  ring_buffer.push(2);
  ring_buffer.push(3); // Buffer: [1, 2, 3]

  auto it = ring_buffer.begin();
  EXPECT_EQ(*it, 1);

  // Pre-increment
  EXPECT_EQ(*(++it), 2);
  EXPECT_EQ(*it, 2);

  // Post-increment
  EXPECT_EQ(*(it++), 2);
  EXPECT_EQ(*it, 3);

  // Pre-decrement
  EXPECT_EQ(*(--it), 2);
  EXPECT_EQ(*it, 2);

  // Post-decrement
  EXPECT_EQ(*(it--), 2);
  EXPECT_EQ(*it, 1);

  // operator+ and operator[]
  EXPECT_EQ(*(ring_buffer.begin() + 1), 2);
  EXPECT_EQ(ring_buffer.begin()[2], 3);

  // operator+=
  it = ring_buffer.begin(); // it points to 1
  it += 2;                  // it points to 3
  EXPECT_EQ(*it, 3);

  // operator- and operator-=
  it -= 1; // it points to 2
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(*(ring_buffer.begin() + 2 - 1), 2); // (1,2,3) -> 3-1=2

  // Comparison operators
  auto it1 = ring_buffer.begin();     // 1
  auto it2 = ring_buffer.begin() + 1; // 2
  auto it3 = ring_buffer.begin() + 2; // 3
  auto it_end = ring_buffer.end();

  EXPECT_TRUE(it1 == ring_buffer.begin());
  EXPECT_FALSE(it1 == it2);
  EXPECT_TRUE(it1 != it2);
  EXPECT_FALSE(it1 != ring_buffer.begin());

  EXPECT_TRUE(it1 < it2);
  EXPECT_TRUE(it1 <= it2);
  EXPECT_FALSE(it1 > it2);
  EXPECT_FALSE(it1 >= it2);

  EXPECT_TRUE(it2 < it3);
  EXPECT_TRUE(it3 > it1);
  EXPECT_TRUE(it_end > it3); // end() is one past the last element

  // Test with wrap-around
  ring_buffer.pop(); // pop 1
  ring_buffer.pop(); // pop 2
  ring_buffer.push(4);
  ring_buffer.push(5);
  ring_buffer.push(6);
  ring_buffer.push(7);
  ring_buffer.push(8);
  ring_buffer.push(9); // Buffer: [3, 4, 5, 6, 7, 8, 9] (size 7)
  // head is at index 2 (value 3), tail is at index 1 (value 9)
  // storage: [9, _, 3, 4, 5, 6, 7, 8]

  auto it_w_begin = ring_buffer.begin(); // points to 3
  auto it_w_end = ring_buffer.end();     // points to 9 (one past 8)

  EXPECT_EQ(*it_w_begin, 3);
  EXPECT_EQ(*(it_w_begin + 1), 4);
  EXPECT_EQ(*(it_w_begin + 6), 9); // 3,4,5,6,7,8,9 (7 elements)
  EXPECT_EQ(it_w_begin[6], 9);

  // Distance
  EXPECT_EQ(it_w_end - it_w_begin, 7); // 7 elements
  EXPECT_EQ(it_w_begin - it_w_end, -7);

  // Negative indexing
  EXPECT_EQ(it_w_end[-1], 9); // Last element
  EXPECT_EQ(it_w_end[-2], 8); // Second to last
}

TEST_F(RingBufferTest, ReverseIterator)
{
  ring_buffer.push(10);
  ring_buffer.push(20);
  ring_buffer.push(30); // Buffer: [10, 20, 30]

  std::vector<byte> expected_reverse = { 30, 20, 10 };
  std::vector<byte> actual_reverse;
  for (auto rit = ring_buffer.rbegin(); rit != ring_buffer.rend(); ++rit) {
    actual_reverse.push_back(*rit);
  }
  EXPECT_EQ(actual_reverse, expected_reverse);

  // Test with wrap-around
  ring_buffer.pop(); // pop 10
  ring_buffer.pop(); // pop 20
  ring_buffer.push(40);
  ring_buffer.push(50);
  ring_buffer.push(60);
  ring_buffer.push(70);
  ring_buffer.push(80);
  ring_buffer.push(90); // Buffer: [30, 40, 50, 60, 70, 80, 90] (size 7)
  // head is at index 2 (value 30), tail is at index 1 (value 90)

  std::vector<byte> expected_reverse_wrapped = { 90, 80, 70, 60, 50, 40, 30 };
  std::vector<byte> actual_reverse_wrapped;
  for (auto rit = ring_buffer.rbegin(); rit != ring_buffer.rend(); ++rit) {
    actual_reverse_wrapped.push_back(*rit);
  }
  EXPECT_EQ(actual_reverse_wrapped, expected_reverse_wrapped);
}

/// @} // end group test
