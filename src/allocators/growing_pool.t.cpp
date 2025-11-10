#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>

// ============================================================================
// Test Fixture
// ============================================================================

class GrowingPoolTest : public ::testing::Test {
protected:
  using local_alloc = local_buffer(16, 128);
  using pool_type = growing_pool(8, 8, local_alloc);

  local_alloc upstream;
  pool_type pool{&upstream};
};

// ============================================================================
// Basic Allocation Tests
// ============================================================================

TEST_F(GrowingPoolTest, BasicAllocation) {
  auto ptr1_result = pool.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;
  ASSERT_NE(ptr1, nullptr);

  auto ptr2_result = pool.allocate_block();
  ASSERT_TRUE(ptr2_result);
  auto ptr2 = *ptr2_result;
  ASSERT_NE(ptr2, nullptr);

  EXPECT_NE(ptr1, ptr2);

  ASSERT_TRUE(pool.deallocate_block(ptr1));
  ASSERT_TRUE(pool.deallocate_block(ptr2));
}

TEST_F(GrowingPoolTest, AllocationDeallocPatterns) {
  // Allocate, deallocate, allocate again pattern
  auto ptr1_result = pool.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;

  ASSERT_TRUE(pool.deallocate_block(ptr1));

  // Should be able to allocate again (reuse)
  auto ptr2_result = pool.allocate_block();
  ASSERT_TRUE(ptr2_result);
  auto ptr2 = *ptr2_result;

  ASSERT_TRUE(pool.deallocate_block(ptr2));
}

// ============================================================================
// Manager Capacity Tests
// ============================================================================

TEST_F(GrowingPoolTest, ManyAllocations) {
  constexpr size_t blocks_per_manager =
      pool_type::manager_type::max_block_count;
  constexpr size_t num_blocks = blocks_per_manager - 2;

  pool_type::pointer_type blocks[num_blocks];

  for (size_t i = 0; i < num_blocks; ++i) {
    auto result = pool.allocate_block();
    ASSERT_TRUE(result) << "Failed to allocate block " << i;
    blocks[i] = *result;
    ASSERT_NE(blocks[i], nullptr);
  }

  for (size_t i = 0; i < num_blocks; ++i) {
    ASSERT_TRUE(pool.deallocate_block(blocks[i]));
  }
}

TEST_F(GrowingPoolTest, SingleManagerExhaustion) {
  using manager_type = pool_type::manager_type;

  constexpr size_t manager_capacity = manager_type::max_block_count;
  std::array<pool_type::pointer_type, manager_capacity> blocks;

  // Exhaust a single manager
  for (size_t i = 0; i < manager_capacity; ++i) {
    auto result = pool.allocate_block();
    ASSERT_TRUE(result);
    blocks.at(i) = *result;
  }

  for (size_t i = 0; i < manager_capacity; ++i) {
    ASSERT_TRUE(pool.deallocate_block(blocks[i]));
  }
}

TEST_F(GrowingPoolTest, MultipleManagerAllocation) {
  using manager_type = pool_type::manager_type;
  constexpr size_t manager_capacity = manager_type::max_block_count;
  constexpr size_t num_blocks = manager_capacity + 10;

  std::array<pool_type::pointer_type, num_blocks> blocks;

  // Allocate across manager boundary
  for (size_t i = 0; i < num_blocks; ++i) {
    auto result = pool.allocate_block();
    ASSERT_TRUE(result) << "Failed at block " << i;
    blocks[i] = *result;
  }

  // Verify all pointers are distinct
  for (size_t i = 0; i < num_blocks - 1; ++i) {
    EXPECT_NE(blocks[i], blocks[i + 1]);
  }

  // Deallocate all
  for (size_t i = 0; i < num_blocks; ++i) {
    ASSERT_TRUE(pool.deallocate_block(blocks[i]));
  }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(GrowingPoolTest, MultiManagerDataIntegrity) {
  using manager_type = pool_type::manager_type;
  constexpr size_t manager_capacity = manager_type::max_block_count;
  constexpr size_t num_blocks = manager_capacity + 5;

  std::array<pool_type::pointer_type, num_blocks> blocks;

  // Allocate blocks across multiple managers
  for (size_t i = 0; i < num_blocks; ++i) {
    auto result = pool.allocate_block();
    ASSERT_TRUE(result);
    blocks[i] = *result;
  }

  // Write unique values to each block
  for (size_t i = 0; i < num_blocks; ++i) {
    auto *block = static_cast<void *>(blocks[i]);
    std::memset(block, static_cast<int>(i & 0xFF), 8);
  }

  // Verify data integrity across all managers
  for (size_t i = 0; i < num_blocks; ++i) {
    auto *block = static_cast<void *>(blocks[i]);
    auto *byte_ptr = static_cast<unsigned char *>(block);
    EXPECT_EQ(byte_ptr[0], static_cast<unsigned char>(i & 0xFF))
        << "Block " << i << " data mismatch";
  }

  // Deallocate in reverse order
  for (int i = num_blocks - 1; i >= 0; --i) {
    ASSERT_TRUE(pool.deallocate_block(blocks[i]));
  }
}

TEST_F(GrowingPoolTest, MultiplePools) {
  // Create a second pool with a unique tag
  growing_pool(8, 8, local_alloc) pool1(&upstream);

  auto ptr1_result = pool1.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;
  ASSERT_NE(ptr1, nullptr);

  // Write to the block
  auto *block1 = static_cast<void *>(ptr1);
  std::memset(block1, 0xCD, 8);

  auto *byte_ptr1 = static_cast<unsigned char *>(block1);
  EXPECT_EQ(byte_ptr1[0], 0xCD);

  ASSERT_TRUE(pool1.deallocate_block(ptr1));
}
