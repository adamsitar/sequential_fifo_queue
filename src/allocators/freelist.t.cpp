#include <freelist.h>
#include <gtest/gtest.h>

// Bare minimum test suite for freelist
constexpr size_t block_size{64};
constexpr size_t block_count{4};
using test_freelist = freelist<block_size, block_count>;

class FreelistTest : public ::testing::Test {
protected:
  test_freelist list;
};

TEST_F(FreelistTest, CanConstruct) { EXPECT_EQ(list.size(), block_count); }

TEST_F(FreelistTest, InitiallyFull) {
  EXPECT_FALSE(list.is_empty());
  EXPECT_TRUE(list.is_full());
}

TEST_F(FreelistTest, CanPop) {
  auto result = list.pop();
  ASSERT_TRUE(result.has_value());
  auto *ptr = result.value();
  EXPECT_NE(ptr, nullptr);
}

TEST_F(FreelistTest, CanPushAndPop) {
  auto result = list.pop();
  ASSERT_TRUE(result.has_value());
  auto *ptr = result.value();
  ASSERT_NE(ptr, nullptr);

  list.push(*ptr);

  EXPECT_TRUE(list.is_full());
}

TEST_F(FreelistTest, CanReset) {
  list.pop();

  list.reset();

  EXPECT_TRUE(list.is_full());
}

// Test that consecutive pops return different blocks
TEST_F(FreelistTest, ConsecutivePopsReturnDistinctBlocks) {
  auto result1 = list.pop();
  auto result2 = list.pop();
  auto result3 = list.pop();

  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  ASSERT_TRUE(result3.has_value());

  auto *ptr1 = result1.value();
  auto *ptr2 = result2.value();
  auto *ptr3 = result3.value();

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);
  ASSERT_NE(ptr3, nullptr);

  // All three should be different
  EXPECT_NE(ptr1, ptr2);
  EXPECT_NE(ptr2, ptr3);
  EXPECT_NE(ptr1, ptr3);
}
