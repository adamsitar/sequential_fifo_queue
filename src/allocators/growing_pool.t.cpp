#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <print>

// ============================================================================
// Growing Pool Test Fixture
// ============================================================================

class GrowingPoolTest : public ::testing::Test {
protected:
  using local_alloc = local_buffer(16, 128);
  using pool_type = growing_pool(8, local_alloc);

  local_alloc upstream;
  pool_type pool{&upstream};
};

// ============================================================================
// Growing Pool Tests
// ============================================================================

TEST_F(GrowingPoolTest, BasicAllocation) {
  // Allocate a block
  auto ptr1_result = pool.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;
  ASSERT_NE(ptr1, nullptr);

  // Allocate another block
  auto ptr2_result = pool.allocate_block();
  ASSERT_TRUE(ptr2_result);
  auto ptr2 = *ptr2_result;
  ASSERT_NE(ptr2, nullptr);

  // Pointers should be different
  EXPECT_NE(ptr1, ptr2);

  // Deallocate blocks
  ASSERT_TRUE(pool.deallocate_block(ptr1));
  ASSERT_TRUE(pool.deallocate_block(ptr2));
}

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

TEST_F(GrowingPoolTest, MultiManagerPointerResolution) {
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

  // Write unique values to each block and verify read-back
  for (size_t i = 0; i < num_blocks; ++i) {
    auto *block = static_cast<void *>(blocks[i]);
    std::memset(block, static_cast<int>(i & 0xFF), 8);
  }

  for (size_t i = 0; i < num_blocks; ++i) {
    auto *block = static_cast<void *>(blocks[i]);
    auto *byte_ptr = static_cast<unsigned char *>(block);
    EXPECT_EQ(byte_ptr[0], static_cast<unsigned char>(i & 0xFF))
        << "Block " << i << " data mismatch";
  }

  // Deallocate in reverse order (stress test)
  for (int i = num_blocks - 1; i >= 0; --i) {
    ASSERT_TRUE(pool.deallocate_block(blocks[i]));
  }
}

TEST_F(GrowingPoolTest, PointerBitPacking) {
  // Verify bit packing is working
  EXPECT_LE(pool_type::pointer_type::storage_bytes(), 2)
      << "Pointer should be 1-2 bytes for this configuration";

  EXPECT_EQ(pool_type::offset_bits, 1);  // 2 blocks per segment
  EXPECT_EQ(pool_type::segment_bits, 2); // ~4 segments per manager
  EXPECT_EQ(pool_type::manager_bits, 7); // 128 max managers

  // Total should be 10 bits
  constexpr size_t total_bits = pool_type::offset_bits +
                                pool_type::segment_bits +
                                pool_type::manager_bits;
  EXPECT_EQ(total_bits, 10);
}

TEST_F(GrowingPoolTest, PointerResolution) {
  auto ptr_result = pool.allocate_block();
  ASSERT_TRUE(ptr_result);
  auto ptr = *ptr_result;

  // Pointer should resolve to valid memory
  ASSERT_NE(ptr, nullptr);

  // Should be able to write/read from the block
  auto *block = static_cast<void *>(ptr);
  std::memset(block, 0xAB, 8);

  auto *byte_ptr = static_cast<unsigned char *>(block);
  EXPECT_EQ(byte_ptr[0], 0xAB);
  EXPECT_EQ(byte_ptr[7], 0xAB);

  ASSERT_TRUE(pool.deallocate_block(ptr));
}

TEST_F(GrowingPoolTest, PointerComparison) {
  auto ptr1_result = pool.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;

  auto ptr2_result = pool.allocate_block();
  ASSERT_TRUE(ptr2_result);
  auto ptr2 = *ptr2_result;

  // Pointers should be comparable
  EXPECT_NE(ptr1, ptr2);
  EXPECT_EQ(ptr1, ptr1);
  EXPECT_EQ(ptr2, ptr2);

  // Null comparison
  pool_type::pointer_type null_ptr{nullptr};
  EXPECT_EQ(null_ptr, nullptr);
  EXPECT_NE(ptr1, nullptr);

  pool.deallocate_block(ptr1);
  pool.deallocate_block(ptr2);
}

TEST_F(GrowingPoolTest, AllocationDeallocPatterns) {
  // Allocate, deallocate, allocate again pattern
  auto ptr1_result = pool.allocate_block();
  ASSERT_TRUE(ptr1_result);
  auto ptr1 = *ptr1_result;

  ASSERT_TRUE(pool.deallocate_block(ptr1));

  // Should be able to allocate again
  auto ptr2_result = pool.allocate_block();
  ASSERT_TRUE(ptr2_result);
  auto ptr2 = *ptr2_result;

  ASSERT_TRUE(pool.deallocate_block(ptr2));
}

// ============================================================================
// Integration Test: Multiple Pools
// ============================================================================

TEST_F(GrowingPoolTest, MultiplePools) {
  // Create a second pool with a unique tag (macro invocation creates unique
  // type)
  growing_pool(8, local_alloc) pool1(&upstream);

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
