#include <array>
#include <gtest/gtest.h>
#include <local_buffer.h>

constexpr size_t block_size{64};
constexpr size_t block_count{4};
using test_buffer = local_buffer(block_size, block_count);

class LocalBufferTest : public ::testing::Test {
protected:
  test_buffer buffer;
};

TEST_F(LocalBufferTest, CanConstruct) { EXPECT_EQ(buffer.size(), block_count); }

TEST_F(LocalBufferTest, CanAllocateBlock) {
  auto ptr = unwrap(buffer.allocate_block());
  EXPECT_NE(ptr, nullptr);
}

TEST_F(LocalBufferTest, CanDeallocateBlock) {
  auto ptr_result = buffer.allocate_block();
  ASSERT_TRUE(ptr_result.has_value());

  auto dealloc_result = buffer.deallocate_block(*ptr_result);
  EXPECT_TRUE(dealloc_result.has_value());
}

TEST_F(LocalBufferTest, CanReset) {
  auto ptr_result = buffer.allocate_block();
  ASSERT_TRUE(ptr_result.has_value());

  buffer.reset();
}

TEST_F(LocalBufferTest, ConsecutiveAllocationsReturnDistinctBlocks) {
  std::array<test_buffer::pointer_type, 3> ptrs{};

  // Allocate three blocks
  for (auto &ptr : ptrs) {
    auto ptr_result = buffer.allocate_block();
    ASSERT_TRUE(ptr_result.has_value());
    ptr = *ptr_result;
    ASSERT_NE(ptr, nullptr);
  }

  // All should be distinct
  for (size_t i = 0; i < ptrs.size(); ++i) {
    for (size_t j = i + 1; j < ptrs.size(); ++j) {
      EXPECT_NE(ptrs[i], ptrs[j]);
    }
  }

  // Cleanup
  for (auto &ptr : ptrs) {
    auto dealloc_result = buffer.deallocate_block(ptr);
    EXPECT_TRUE(dealloc_result.has_value());
  }
}

TEST_F(LocalBufferTest, CanAllocateUpToCapacity) {
  std::array<test_buffer::pointer_type, block_count> ptrs{};

  // Allocate all blocks
  for (auto &ptr : ptrs) {
    auto ptr_result = buffer.allocate_block();
    ASSERT_TRUE(ptr_result.has_value());
    ptr = *ptr_result;
    EXPECT_NE(ptr, nullptr);
  }

  // All should be distinct
  for (size_t i = 0; i < ptrs.size(); ++i) {
    for (size_t j = i + 1; j < ptrs.size(); ++j) {
      EXPECT_NE(ptrs[i], ptrs[j]);
    }
  }

  // Cleanup
  for (auto &ptr : ptrs) {
    auto dealloc_result = buffer.deallocate_block(ptr);
    EXPECT_TRUE(dealloc_result.has_value());
  }
}

TEST_F(LocalBufferTest, CanReallocateAfterDeallocation) {
  auto ptr1_result = buffer.allocate_block();
  ASSERT_TRUE(ptr1_result.has_value());
  auto ptr1 = *ptr1_result;

  auto dealloc1_result = buffer.deallocate_block(ptr1);
  EXPECT_TRUE(dealloc1_result.has_value());

  auto ptr2_result = buffer.allocate_block();
  ASSERT_TRUE(ptr2_result.has_value());
  auto ptr2 = *ptr2_result;

  // After deallocation and reallocation, we should get a valid block
  // (might be the same block, which is fine)
  EXPECT_NE(ptr2, nullptr);

  auto dealloc2_result = buffer.deallocate_block(ptr2);
  EXPECT_TRUE(dealloc2_result.has_value());
}

// ============================================================================
// Error Case Tests
// ============================================================================

TEST_F(LocalBufferTest, AllocationFailsWhenExhausted) {
  std::array<test_buffer::pointer_type, block_count> ptrs{};

  // Allocate all blocks
  for (auto &ptr : ptrs) {
    auto ptr_result = buffer.allocate_block();
    ASSERT_TRUE(ptr_result.has_value());
    ptr = *ptr_result;
  }

  // Try to allocate one more - should fail
  auto extra_result = buffer.allocate_block();
  EXPECT_FALSE(extra_result.has_value());
  EXPECT_EQ(extra_result.error(), error::generic);

  // Cleanup
  for (auto &ptr : ptrs) {
    auto dealloc_result = buffer.deallocate_block(ptr);
    EXPECT_TRUE(dealloc_result.has_value());
  }
}

TEST_F(LocalBufferTest, DeallocateNullPtrReturnsError) {
  auto dealloc_result = buffer.deallocate_block(nullptr);
  EXPECT_FALSE(dealloc_result.has_value());
  EXPECT_EQ(dealloc_result.error(), error::generic);
}
