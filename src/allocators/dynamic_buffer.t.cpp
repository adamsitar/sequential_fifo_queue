#include <array>
#include <dynamic_buffer.h>
#include <gtest/gtest.h>
#include <local_buffer.h>
#include <vector>

/*
First Half - Core Functionality (5 tests):
1. CanConstruct - Verifies basic construction
2. CanAllocateSingleBlock - Single allocation
3. CanAllocateAndDeallocate - Basic lifecycle
4. CanAllocateMultipleBlocks - Multiple allocations with uniqueness
verification
5. CreatesNewSegmentWhenFull - Validates segment expansion behavior

Second Half - Segmented Pointer Tests (5 tests):
6. SegmentedPtrNullHandling - Null pointer semantics
7. SegmentedPtrConversionRoundtrip - Raw pointer ↔ segmented pointer
conversion
8. SegmentedPtrComparison - Equality and ordering operators
9. SegmentedPtrAccessorMethods - Tests get_segment_id() and get_offset()
10. TwoBuffersDistinctSegmentedPtrs - The comprehensive multi-buffer test
*/

// Configuration
constexpr size_t upstream_block_size{2048};
constexpr size_t upstream_block_count{16};
constexpr size_t dynamic_block_size{256};

using upstream_buffer = local_buffer(upstream_block_size, upstream_block_count);
using test_buffer = dynamic_buffer(dynamic_block_size, upstream_buffer);

class DynamicBufferTest : public ::testing::Test {
protected:
  upstream_buffer upstream;
  test_buffer buffer{&upstream};
};

// ============================================================================
// First Half: Basic dynamic_buffer Functionality
// ============================================================================

TEST_F(DynamicBufferTest, CanConstruct) {
  // Construction should succeed without allocating segments
  EXPECT_EQ(upstream.size(), upstream_block_count);
}

TEST_F(DynamicBufferTest, CanAllocateSingleBlock) {
  auto ptr_result = buffer.allocate_block();
  ASSERT_TRUE(ptr_result.has_value());
  EXPECT_NE(*ptr_result, nullptr);
}

TEST_F(DynamicBufferTest, CanAllocateAndDeallocate) {
  auto ptr_result = buffer.allocate_block();
  ASSERT_TRUE(ptr_result.has_value());
  ASSERT_NE(*ptr_result, nullptr);

  auto dealloc_result = buffer.deallocate_block(*ptr_result);
  EXPECT_TRUE(dealloc_result.has_value());
}

TEST_F(DynamicBufferTest, CanAllocateMultipleBlocks) {
  constexpr size_t num_blocks = 4;
  std::array<test_buffer::pointer_type, num_blocks> ptrs{};

  // Allocate multiple blocks
  for (auto &ptr : ptrs) {
    auto ptr_result = buffer.allocate_block();
    ASSERT_TRUE(ptr_result.has_value());
    ptr = *ptr_result;
    EXPECT_NE(ptr, nullptr);
  }

  // All pointers should be distinct
  for (size_t i = 0; i < ptrs.size(); ++i) {
    for (size_t j = i + 1; j < ptrs.size(); ++j) {
      EXPECT_NE(ptrs[i], ptrs[j]);
    }
  }

  // Deallocate all
  for (auto &ptr : ptrs) {
    auto dealloc_result = buffer.deallocate_block(ptr);
    EXPECT_TRUE(dealloc_result.has_value());
  }
}

TEST_F(DynamicBufferTest, CreatesNewSegmentWhenFull) {
  constexpr size_t blocks_per_segment =
      upstream_block_size / dynamic_block_size;
  std::vector<test_buffer::pointer_type> ptrs;

  // Allocate enough blocks to fill first segment and spill into second
  for (size_t i = 0; i < blocks_per_segment + 2; ++i) {
    auto ptr_result = buffer.allocate_block();
    ASSERT_TRUE(ptr_result.has_value());
    ASSERT_NE(*ptr_result, nullptr);
    ptrs.push_back(*ptr_result);
  }

  // Verify that at least two distinct segments are being used
  bool found_different_segment = false;
  auto first_segment = ptrs[0].get_segment_id();
  for (const auto &ptr : ptrs) {
    if (ptr.get_segment_id() != first_segment) {
      found_different_segment = true;
      break;
    }
  }

  EXPECT_TRUE(found_different_segment);

  // Cleanup
  for (auto &ptr : ptrs) {
    auto dealloc_result = buffer.deallocate_block(ptr);
    EXPECT_TRUE(dealloc_result.has_value());
  }
}

// ============================================================================
// Second Half: Segmented Pointer Tests
// ============================================================================

TEST_F(DynamicBufferTest, SegmentedPtrNullHandling) {
  // Default constructed pointer should be null
  test_buffer::pointer_type null_ptr{nullptr};
  EXPECT_EQ(null_ptr, nullptr);
  EXPECT_TRUE(static_cast<void *>(null_ptr) == nullptr);

  // Allocate a real block
  auto valid_ptr_result = buffer.allocate_block();
  ASSERT_TRUE(valid_ptr_result.has_value());
  EXPECT_NE(*valid_ptr_result, nullptr);

  // Cleanup
  auto dealloc_result = buffer.deallocate_block(*valid_ptr_result);
  EXPECT_TRUE(dealloc_result.has_value());
}

TEST_F(DynamicBufferTest, SegmentedPtrConversionRoundtrip) {
  auto seg_ptr_result = buffer.allocate_block();
  ASSERT_TRUE(seg_ptr_result.has_value());
  auto seg_ptr = *seg_ptr_result;
  ASSERT_NE(seg_ptr, nullptr);

  // Convert to raw pointer
  void *raw_ptr = static_cast<void *>(seg_ptr);
  ASSERT_NE(raw_ptr, nullptr);

  // Convert back to segmented pointer (must cast to block_type*)
  auto *block_ptr = static_cast<test_buffer::block_type *>(raw_ptr);
  test_buffer::pointer_type reconstructed{block_ptr};
  EXPECT_NE(reconstructed, nullptr);

  // Should point to the same location
  EXPECT_EQ(static_cast<void *>(reconstructed), raw_ptr);
  EXPECT_EQ(seg_ptr.get_segment_id(), reconstructed.get_segment_id());
  EXPECT_EQ(seg_ptr.get_offset(), reconstructed.get_offset());

  // Cleanup
  auto dealloc_result = buffer.deallocate_block(seg_ptr);
  EXPECT_TRUE(dealloc_result.has_value());
}

TEST_F(DynamicBufferTest, SegmentedPtrComparison) {
  auto ptr1_result = buffer.allocate_block();
  auto ptr2_result = buffer.allocate_block();
  ASSERT_TRUE(ptr1_result.has_value());
  ASSERT_TRUE(ptr2_result.has_value());
  auto ptr1 = *ptr1_result;
  auto ptr2 = *ptr2_result;
  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  // Different allocations should not be equal
  EXPECT_NE(ptr1, ptr2);

  // Pointer should be equal to itself
  EXPECT_EQ(ptr1, ptr1);
  EXPECT_EQ(ptr2, ptr2);

  // Comparison operators should work
  bool has_ordering = (ptr1 < ptr2) || (ptr2 < ptr1);
  EXPECT_TRUE(has_ordering);

  // Cleanup
  EXPECT_TRUE(buffer.deallocate_block(ptr1).has_value());
  EXPECT_TRUE(buffer.deallocate_block(ptr2).has_value());
}

TEST_F(DynamicBufferTest, SegmentedPtrAccessorMethods) {
  auto ptr_result = buffer.allocate_block();
  ASSERT_TRUE(ptr_result.has_value());
  auto ptr = *ptr_result;
  ASSERT_NE(ptr, nullptr);

  // Segment ID and offset should be accessible
  auto segment_id = ptr.get_segment_id();
  auto offset = ptr.get_offset();

  // Construct new pointer from segment ID and offset
  test_buffer::pointer_type reconstructed{segment_id, offset};

  // Should point to same location
  EXPECT_EQ(static_cast<void *>(ptr), static_cast<void *>(reconstructed));

  // Cleanup
  EXPECT_TRUE(buffer.deallocate_block(ptr).has_value());
}

TEST_F(DynamicBufferTest, TwoBuffersDistinctSegmentedPtrs) {
  // Create second upstream buffer with different tag
  using upstream_buffer2 =
      local_buffer(upstream_block_size, upstream_block_count);
  using test_buffer2 = dynamic_buffer(dynamic_block_size, upstream_buffer2);

  upstream_buffer2 upstream2;
  test_buffer2 buffer2{&upstream2};

  // Allocate from both buffers
  auto ptr1_result = buffer.allocate_block();
  auto ptr2_result = buffer2.allocate_block();

  ASSERT_TRUE(ptr1_result.has_value());
  ASSERT_TRUE(ptr2_result.has_value());
  auto ptr1 = *ptr1_result;
  auto ptr2 = *ptr2_result;

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  // Convert both to raw pointers
  void *raw1 = static_cast<void *>(ptr1);
  void *raw2 = static_cast<void *>(ptr2);

  EXPECT_NE(raw1, nullptr);
  EXPECT_NE(raw2, nullptr);
  EXPECT_NE(raw1, raw2); // Should be different memory locations

  // Each segmented pointer should only convert pointers from its own buffer
  // Attempting to construct a segmented_ptr from the other buffer's raw pointer
  // should result in null (pointer doesn't belong to this allocator)
  test_buffer::pointer_type cross_convert1{
      static_cast<test_buffer::block_type *>(raw2)};
  EXPECT_EQ(cross_convert1, nullptr);

  test_buffer2::pointer_type cross_convert2{
      static_cast<test_buffer2::block_type *>(raw1)};
  EXPECT_EQ(cross_convert2, nullptr);

  // But converting their own pointers should work
  test_buffer::pointer_type own_convert1{
      static_cast<test_buffer::block_type *>(raw1)};
  EXPECT_NE(own_convert1, nullptr);
  EXPECT_EQ(static_cast<void *>(own_convert1), raw1);

  test_buffer2::pointer_type own_convert2{
      static_cast<test_buffer2::block_type *>(raw2)};
  EXPECT_NE(own_convert2, nullptr);
  EXPECT_EQ(static_cast<void *>(own_convert2), raw2);

  // Allocate multiple blocks from each buffer to test segment handling
  std::array<test_buffer::pointer_type, 3> ptrs_from_buffer1{};
  std::array<test_buffer2::pointer_type, 3> ptrs_from_buffer2{};

  for (auto &ptr : ptrs_from_buffer1) {
    auto result = buffer.allocate_block();
    ASSERT_TRUE(result.has_value());
    ptr = *result;
    EXPECT_NE(ptr, nullptr);
  }

  for (auto &ptr : ptrs_from_buffer2) {
    auto result = buffer2.allocate_block();
    ASSERT_TRUE(result.has_value());
    ptr = *result;
    EXPECT_NE(ptr, nullptr);
  }

  // Verify all pointers from buffer1 can be converted via their own buffer
  for (const auto &ptr : ptrs_from_buffer1) {
    void *raw = static_cast<void *>(ptr);
    test_buffer::pointer_type reconverted{
        static_cast<test_buffer::block_type *>(raw)};
    EXPECT_EQ(static_cast<void *>(reconverted), raw);
  }

  // Verify all pointers from buffer2 can be converted via their own buffer
  for (const auto &ptr : ptrs_from_buffer2) {
    void *raw = static_cast<void *>(ptr);
    test_buffer2::pointer_type reconverted{
        static_cast<test_buffer2::block_type *>(raw)};
    EXPECT_EQ(static_cast<void *>(reconverted), raw);
  }

  // Cleanup
  EXPECT_TRUE(buffer.deallocate_block(ptr1).has_value());
  EXPECT_TRUE(buffer2.deallocate_block(ptr2).has_value());

  for (auto &ptr : ptrs_from_buffer1) {
    EXPECT_TRUE(buffer.deallocate_block(ptr).has_value());
  }

  for (auto &ptr : ptrs_from_buffer2) {
    EXPECT_TRUE(buffer2.deallocate_block(ptr).has_value());
  }
}
