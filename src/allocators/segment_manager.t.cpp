#include <gtest/gtest.h>
#include <local_buffer.h>
#include <segment_manager.h>

// ============================================================================
// Segment Manager Test Fixture
// ============================================================================

class SegmentManagerTest : public ::testing::Test {
protected:
  using local_alloc = local_buffer(16, 128);
  using seg_manager = segment_manager<8, local_alloc>;

  local_alloc upstream;
  seg_manager manager;

  void TearDown() override { manager.cleanup(&upstream); }
};

// ============================================================================
// Segment Manager Tests
// ============================================================================

TEST_F(SegmentManagerTest, BasicAllocation) {
  // Allocate a block
  auto *block1 = unwrap(manager.try_allocate(&upstream));
  ASSERT_NE(block1, nullptr);

  // Allocate another block
  auto *block2 = unwrap(manager.try_allocate(&upstream));
  ASSERT_NE(block2, nullptr);

  // Blocks should be different
  ASSERT_NE(block1, block2);

  // Manager should own both blocks
  ASSERT_TRUE(manager.owns(block1));
  ASSERT_TRUE(manager.owns(block2));

  // Deallocate blocks
  ASSERT_TRUE(manager.deallocate(block1, &upstream));
  ASSERT_TRUE(manager.deallocate(block2, &upstream));
}

TEST_F(SegmentManagerTest, MultipleSegments) {
  constexpr size_t blocks_per_segment =
      local_alloc::block_size / seg_manager::block_size;
  constexpr size_t num_blocks = blocks_per_segment * 2 + 1;

  std::array<seg_manager::block_type *, num_blocks> blocks;

  for (size_t i = 0; i < num_blocks; ++i) {
    blocks[i] = unwrap(manager.try_allocate(&upstream));
    ASSERT_NE(blocks[i], nullptr) << "Failed to allocate block " << i;
  }

  EXPECT_EQ(manager.segment_count(), 3);

  for (size_t i = 0; i < num_blocks; ++i) {
    ASSERT_TRUE(manager.deallocate(blocks[i], &upstream));
  }
}

TEST_F(SegmentManagerTest, Exhaustion) {
  constexpr size_t total_capacity = seg_manager::max_block_count;
  std::array<seg_manager::block_type *, total_capacity> blocks;

  for (size_t i = 0; i < total_capacity; ++i) {
    auto result = manager.try_allocate(&upstream);
    ASSERT_TRUE(result) << "Failed to allocate block " << i
                        << " within capacity";
    blocks[i] = *result;
    ASSERT_NE(blocks[i], nullptr);
  }

  EXPECT_EQ(manager.segment_count(), seg_manager::max_segments);

  auto overflow_result = manager.try_allocate(&upstream);
  EXPECT_FALSE(overflow_result)
      << "Should fail when allocating beyond capacity";

  for (size_t i = 0; i < total_capacity; ++i) {
    ASSERT_TRUE(manager.deallocate(blocks[i], &upstream));
  }
}

TEST_F(SegmentManagerTest, OwnershipCheck) {
  seg_manager manager2;

  auto block1 = unwrap(manager.try_allocate(&upstream));
  ASSERT_NE(block1, nullptr);

  auto block2 = unwrap(manager2.try_allocate(&upstream));
  ASSERT_NE(block2, nullptr);

  // Each manager should only own its own block
  EXPECT_TRUE(manager.owns(block1));
  EXPECT_FALSE(manager.owns(block2));

  EXPECT_FALSE(manager2.owns(block1));
  EXPECT_TRUE(manager2.owns(block2));

  manager.deallocate(block1, &upstream);
  manager2.deallocate(block2, &upstream);

  manager2.cleanup(&upstream);
}
