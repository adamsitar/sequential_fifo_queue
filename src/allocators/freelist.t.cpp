#include <freelist.h>
#include <gtest/gtest.h>

// Bare minimum test suite for freelist
constexpr size_t block_size{64};
constexpr size_t block_count{4};
using test_freelist = freelist<block_size, block_count, decltype([] {})>;

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
  // Reference is valid if result has value - no nullptr check needed
}

TEST_F(FreelistTest, CanPushAndPop) {
  auto result = list.pop();
  ASSERT_TRUE(result.has_value());
  auto &res = result.value();

  list.push(res);

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

  auto &res1 = result1.value();
  auto &res2 = result2.value();
  auto &res3 = result3.value();

  // All three should be different blocks (compare addresses)
  EXPECT_NE(&res1, &res2);
  EXPECT_NE(&res2, &res3);
  EXPECT_NE(&res1, &res3);
}
