#include <growing_pool.h>
#include <gtest/gtest.h>
#include <local_buffer.h>

// ============================================================================
// Test Fixture
// ============================================================================

class GrowingPoolPtrTest : public ::testing::Test {
protected:
  using local_alloc = local_buffer(16, 128);
  using pool_type = growing_pool(8, 8, local_alloc);
  using ptr_type = pool_type::pointer_type;

  local_alloc upstream;
  pool_type pool{&upstream};
};

// ============================================================================
// Construction & Null Tests
// ============================================================================

TEST_F(GrowingPoolPtrTest, DefaultConstructorIsNull) {
  ptr_type ptr;
  EXPECT_EQ(ptr, nullptr);
  EXPECT_FALSE(ptr);
}

TEST_F(GrowingPoolPtrTest, NullptrConstructorIsNull) {
  ptr_type ptr{nullptr};
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(GrowingPoolPtrTest, ValidIDConstruction) {
  ptr_type ptr(0, 0, 0);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(ptr.get_manager_id(), 0);
  EXPECT_EQ(ptr.get_segment_id(), 0);
  EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(GrowingPoolPtrTest, GettersOnNullFatal) {
  ptr_type null_ptr;
  EXPECT_DEATH(
      { null_ptr.get_manager_id(); },
      "cannot get manager_id from null pointer");
  EXPECT_DEATH(
      { null_ptr.get_segment_id(); },
      "cannot get segment_id from null pointer");
  EXPECT_DEATH(
      { null_ptr.get_offset(); }, "cannot get offset from null pointer");
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(GrowingPoolPtrTest, RejectsOutOfRangeOffset) {
  EXPECT_DEATH(
      { ptr_type ptr(0, 0, ptr_type::max_offset_index + 1); },
      "offset out of range");
}

TEST_F(GrowingPoolPtrTest, RejectsOutOfRangeSegment) {
  EXPECT_DEATH(
      { ptr_type ptr(0, ptr_type::max_segment_index + 1, 0); },
      "segment_id out of range");
}

TEST_F(GrowingPoolPtrTest, RejectsOutOfRangeManager) {
  EXPECT_DEATH(
      { ptr_type ptr(ptr_type::max_manager_index + 1, 0, 0); },
      "manager_id out of range");
}

TEST_F(GrowingPoolPtrTest, AcceptsMaxValidIDs) {
  EXPECT_NO_THROW({
    ptr_type ptr(ptr_type::max_manager_index, ptr_type::max_segment_index,
                 ptr_type::max_offset_index);
  });
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_F(GrowingPoolPtrTest, NullComparison) {
  ptr_type null1;
  ptr_type null2{nullptr};
  EXPECT_EQ(null1, null2);
  EXPECT_EQ(null1, nullptr);
}

TEST_F(GrowingPoolPtrTest, NullSortsBeforeValid) {
  ptr_type null_ptr;
  ptr_type valid_ptr(0, 0, 0);
  EXPECT_LT(null_ptr, valid_ptr);
}

TEST_F(GrowingPoolPtrTest, LexicographicOrdering) {
  ptr_type ptr_000(0, 0, 0);
  ptr_type ptr_001(0, 0, 1);
  ptr_type ptr_010(0, 1, 0);
  ptr_type ptr_100(1, 0, 0);

  // Same manager and segment, different offset
  EXPECT_LT(ptr_000, ptr_001);
  // Same manager, different segment
  EXPECT_LT(ptr_001, ptr_010);
  // Different manager
  EXPECT_LT(ptr_010, ptr_100);
}

// ============================================================================
// Pointer Arithmetic Tests
// ============================================================================

TEST_F(GrowingPoolPtrTest, IncrementWithinSegment) {
  ptr_type ptr(0, 0, 0);

  ++ptr;
  EXPECT_EQ(ptr.get_manager_id(), 0);
  EXPECT_EQ(ptr.get_segment_id(), 0);
  EXPECT_EQ(ptr.get_offset(), 1);
}

TEST_F(GrowingPoolPtrTest, IncrementCrossesSegmentBoundary) {
  ptr_type ptr(0, 0, ptr_type::max_offset_index);

  ++ptr;
  EXPECT_EQ(ptr.get_manager_id(), 0);
  EXPECT_EQ(ptr.get_segment_id(), 1); // Should move to next segment
  EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(GrowingPoolPtrTest, DecrementWithinSegment) {
  ptr_type ptr(0, 0, 1);

  --ptr;
  EXPECT_EQ(ptr.get_manager_id(), 0);
  EXPECT_EQ(ptr.get_segment_id(), 0);
  EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(GrowingPoolPtrTest, DecrementCrossesSegmentBoundary) {
  // Start at first offset in segment 1
  ptr_type ptr(0, 1, 0);

  --ptr;
  EXPECT_EQ(ptr.get_manager_id(), 0);
  EXPECT_EQ(ptr.get_segment_id(), 0); // Should move to previous segment
  EXPECT_EQ(ptr.get_offset(), ptr_type::max_offset_index);
}

TEST_F(GrowingPoolPtrTest, ArithmeticOnNullIsNoop) {
  ptr_type null_ptr;

  ++null_ptr;
  EXPECT_EQ(null_ptr, nullptr);

  --null_ptr;
  EXPECT_EQ(null_ptr, nullptr);

  null_ptr += 5;
  EXPECT_EQ(null_ptr, nullptr);
}

TEST_F(GrowingPoolPtrTest, AddOffset) {
  ptr_type ptr(0, 0, 0);
  ptr += 2; // Should cross into next segment (2 blocks per segment)

  EXPECT_EQ(ptr.get_segment_id(), 1);
  EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(GrowingPoolPtrTest, SubtractOffset) {
  ptr_type ptr(0, 1, 0);
  ptr -= 2; // Should cross back to previous segment

  EXPECT_EQ(ptr.get_segment_id(), 0);
  EXPECT_EQ(ptr.get_offset(), 0);
}

TEST_F(GrowingPoolPtrTest, ArithmeticDetectsUnderflow) {
  ptr_type ptr(0, 0, 0);
  EXPECT_DEATH({ ptr -= 1; }, "pointer arithmetic underflow");
}

TEST_F(GrowingPoolPtrTest, ArithmeticDetectsCrossPoolBoundary) {
  ptr_type ptr(0, 0, 0);

  // Try to advance beyond the entire pool capacity
  EXPECT_DEATH(
      { ptr += pool_type::max_block_count; },
      "beyond end of pool");
}

// ============================================================================
// Allocation & Resolution Tests
// ============================================================================

TEST_F(GrowingPoolPtrTest, AllocateAndResolve) {
  auto result = pool.allocate_block();
  ASSERT_TRUE(result);

  ptr_type ptr = *result;
  EXPECT_NE(ptr, nullptr);

  // Should resolve to valid memory
  void *resolved = static_cast<void *>(ptr);
  EXPECT_NE(resolved, nullptr);

  pool.deallocate_block(ptr);
}

TEST_F(GrowingPoolPtrTest, NullResolvesToNullptr) {
  ptr_type null_ptr;
  void *resolved = static_cast<void *>(null_ptr);
  EXPECT_EQ(resolved, nullptr);
}

TEST_F(GrowingPoolPtrTest, WriteReadThroughPointer) {
  auto result = pool.allocate_block();
  ASSERT_TRUE(result);
  ptr_type ptr = *result;

  // Write and read back
  auto *bytes = static_cast<unsigned char *>(static_cast<void *>(ptr));
  bytes[0] = 0xAB;
  bytes[7] = 0xCD;

  EXPECT_EQ(bytes[0], 0xAB);
  EXPECT_EQ(bytes[7], 0xCD);

  pool.deallocate_block(ptr);
}

TEST_F(GrowingPoolPtrTest, ConstructFromRawPointer) {
  auto result = pool.allocate_block();
  ASSERT_TRUE(result);
  ptr_type ptr1 = *result;

  void *raw = static_cast<void *>(ptr1);
  ptr_type ptr2{raw};

  EXPECT_EQ(ptr1.get_manager_id(), ptr2.get_manager_id());
  EXPECT_EQ(ptr1.get_segment_id(), ptr2.get_segment_id());
  EXPECT_EQ(ptr1.get_offset(), ptr2.get_offset());

  pool.deallocate_block(ptr1);
}

TEST_F(GrowingPoolPtrTest, ConstructFromNullRawPointer) {
  void *raw_null = nullptr;
  ptr_type ptr{raw_null};
  EXPECT_EQ(ptr, nullptr);
}

TEST_F(GrowingPoolPtrTest, MultipleAllocationsAreDistinct) {
  std::vector<ptr_type> ptrs;

  for (size_t i = 0; i < 5; ++i) {
    auto result = pool.allocate_block();
    ASSERT_TRUE(result);
    ptrs.push_back(*result);
  }

  // All should be distinct
  for (size_t i = 0; i < ptrs.size(); ++i) {
    for (size_t j = i + 1; j < ptrs.size(); ++j) {
      EXPECT_NE(ptrs[i], ptrs[j]);
    }
  }

  for (auto &ptr : ptrs) {
    pool.deallocate_block(ptr);
  }
}
