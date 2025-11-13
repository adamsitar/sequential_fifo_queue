#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <offset_list.h>

constexpr size_t top_level_block_size = 16;
constexpr size_t top_level_block_count = 128;
constexpr size_t node_block_size = 8;
using top_level_allocator = local_buffer(top_level_block_size,
                                         top_level_block_count);
using test_allocator = growing_pool(node_block_size, 32, top_level_allocator);
using test_list = offset_list<int, test_allocator>;

class OffsetListTest : public ::testing::Test {
protected:
  top_level_allocator top_allocator;
  test_allocator allocator{&top_allocator};
  test_list list{&allocator};
};

TEST_F(OffsetListTest, InitiallyEmpty) {
  EXPECT_TRUE(list.is_empty());
  EXPECT_EQ(list.size(), 0);
}

TEST_F(OffsetListTest, PushFrontAddsElement) {
  auto result = list.push_front(42);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(list.is_empty());
  EXPECT_EQ(list.size(), 1);
}

TEST_F(OffsetListTest, PopFrontRemovesElement) {
  list.push_front(42);
  auto result = list.pop_front();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
  EXPECT_TRUE(list.is_empty());
}

TEST_F(OffsetListTest, PopFrontFromEmptyListFails) {
  auto result = list.pop_front();
  EXPECT_FALSE(result.has_value());
}

TEST_F(OffsetListTest, FrontReturnsFirstElement) {
  list.push_front(10);
  list.push_front(20);
  list.push_front(30);

  auto result = list.front();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result.value(), 30);
}

TEST_F(OffsetListTest, FrontOnEmptyListFails) {
  auto result = list.front();
  EXPECT_FALSE(result.has_value());
}

TEST_F(OffsetListTest, PushFrontMaintainsLIFOOrder) {
  list.push_front(1);
  list.push_front(2);
  list.push_front(3);

  EXPECT_EQ(list.size(), 3);
  EXPECT_EQ(list.pop_front().value(), 3);
  EXPECT_EQ(list.pop_front().value(), 2);
  EXPECT_EQ(list.pop_front().value(), 1);
}

TEST_F(OffsetListTest, SizeTracksCorrectly) {
  EXPECT_EQ(list.size(), 0);
  list.push_front(1);
  EXPECT_EQ(list.size(), 1);
  list.push_front(2);
  EXPECT_EQ(list.size(), 2);
  list.pop_front();
  EXPECT_EQ(list.size(), 1);
  list.pop_front();
  EXPECT_EQ(list.size(), 0);
}

TEST_F(OffsetListTest, EmplaceFrontConstructsInPlace) {
  auto result = list.emplace_front(42);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(*list.front().value(), 42);
}

TEST_F(OffsetListTest, ClearEmptiesList) {
  list.push_front(1);
  list.push_front(2);
  list.push_front(3);

  list.clear();

  EXPECT_TRUE(list.is_empty());
  EXPECT_EQ(list.size(), 0);
}

TEST_F(OffsetListTest, BeginEqualsEndForEmptyList) {
  EXPECT_EQ(list.begin(), list.end());
}

TEST_F(OffsetListTest, IteratorTraversesElements) {
  list.push_front(3);
  list.push_front(2);
  list.push_front(1);

  std::vector<int> values;
  for (auto it = list.begin(); it != list.end(); ++it) {
    std::println("{}", *it);
    values.push_back(*it);
  }

  ASSERT_EQ(values.size(), 3);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 3);
}

TEST_F(OffsetListTest, BeforeBeginIterator) {
  list.push_front(1);

  auto it = list.before_begin();
  EXPECT_NE(it, list.begin());

  ++it;
  EXPECT_EQ(it, list.begin());
  EXPECT_EQ(*it, 1);
}

TEST_F(OffsetListTest, InsertAfterBeforeBegin) {
  auto it = list.insert_after(list.before_begin(), 42);
  EXPECT_NE(it, list.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(*list.front().value(), 42);
}

TEST_F(OffsetListTest, InsertAfterInMiddle) {
  list.push_front(3);
  list.push_front(1);

  auto it = list.begin();
  it = list.insert_after(it, 2);

  EXPECT_EQ(*it, 2);
  EXPECT_EQ(list.size(), 3);

  // Verify order: 1, 2, 3
  it = list.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
}

TEST_F(OffsetListTest, EmplaceAfterBeforeBegin) {
  auto it = list.emplace_after(list.before_begin(), 42);
  EXPECT_NE(it, list.end());
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(list.size(), 1);
}

TEST_F(OffsetListTest, EraseAfterBeforeBeginRemovesFirst) {
  list.push_front(2);
  list.push_front(1);

  auto it = list.erase_after(list.before_begin());
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(list.size(), 1);
  EXPECT_EQ(*list.front().value(), 2);
}

TEST_F(OffsetListTest, EraseAfterInMiddle) {
  list.push_front(3);
  list.push_front(2);
  list.push_front(1);

  auto it = list.begin();
  it = list.erase_after(it); // Erase 2

  EXPECT_EQ(*it, 3);
  EXPECT_EQ(list.size(), 2);

  // Verify order: 1, 3
  it = list.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 3);
}

TEST_F(OffsetListTest, EraseAfterRange) {
  list.push_front(4);
  list.push_front(3);
  list.push_front(2);
  list.push_front(1);

  auto first = list.begin();
  auto last = first;
  ++last;
  ++last;
  ++last; // Points to 4

  list.erase_after(first, last); // Erase 2 and 3

  EXPECT_EQ(list.size(), 2);

  // Verify order: 1, 4
  auto it = list.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 4);
}

TEST_F(OffsetListTest, CanHandleMultipleAllocations) {
  // const int num_elements = test_allocator::max_block_count - 2;
  const int num_elements = 32;
  for (int i = 0; i < num_elements; ++i) {
    std::println("pushing i: {}", i);
    auto result = list.push_front(i);
    ASSERT_TRUE(result.has_value()) << "Failed to allocate element " << i;
  }

  EXPECT_EQ(list.size(), num_elements);

  int expected = num_elements - 1;
  for (auto it = list.begin(); it != list.end(); ++it, --expected) {
    EXPECT_EQ(*it, expected);
  }
}
