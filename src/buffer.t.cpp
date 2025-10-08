#include <buffer.h>
#include <cstddef>
#include <gtest/gtest.h>
#include <new>

/**
 * @defgroup test Tests
 * @ingroup buffer
 *
 * This test suite verifies that mbb::Buffer behaves properly:
 * - Allocations and deallocations within block size.
 * - Fallback behavior for large allocations.
 * - Robustness against edge cases and resource exhaustion.
 * - Correctness of equality semantics.
 */

/// @addtogroup test
/// @{

/**
 * @brief Test fixture, constructs with std::pmr::new_delete_resource upstream
 */
class BufferTest : public testing::Test
{
public:
  size_t _max_align{ alignof(std::max_align_t) };
  Buffer<64, 4> _buffer;
};

/**
 * @brief Basic allocation and deallocation works.
 *
 * Allocates a block smaller than the block size,
 * confirms returned pointer is not null,
 * and deallocates without error.
 */
TEST_F(BufferTest, BasicAllocation)
{
  void* p = _buffer.allocate(32);
  ASSERT_NE(p, nullptr);

  _buffer.deallocate(p, 32);
}

/**
 * @brief Allocation exceeding block size falls back upstream.
 *
 * Requests a larger allocation than block size
 * ensures allocation succeeds,
 * then deallocates correctly through fallback.
 */
TEST_F(BufferTest, AllocationLargeFallsBack)
{
  void* p = _buffer.allocate(128);
  ASSERT_NE(p, nullptr);

  _buffer.deallocate(p, 128);
}

/**
 * @brief Exhaustion triggers fallback allocator.
 *
 * Allocate all,
 * verify allocations succeed,
 * next allocation falls back upstream,
 * after deallocation a block can be reused.
 */
TEST_F(BufferTest, ExhaustBuffer)
{
  std::array<void*, 4> blocks{};

  for (auto& block : blocks) {
    block = _buffer.allocate(64);
    ASSERT_NE(block, nullptr);
  }

  EXPECT_THROW(_buffer.allocate(64), std::bad_alloc);

  _buffer.deallocate(blocks[0], 64);
  void* p2 = _buffer.allocate(32);
  ASSERT_EQ(p2, blocks[0]);
}

/**
 * @brief Allocations of size zero throw @c std::bad_alloc.
 */
TEST_F(BufferTest, ZeroByteAllocationThrows)
{
  ASSERT_DEATH(_buffer.allocate(0), "");
}

/**
 * @brief Deallocation of pointers outside buffer falls back upstream.
 *
 * Tests that deallocating a pointer allocated via global new
 * and outside of buffer memory, is forwarded deallocation upstream.
 */
TEST_F(BufferTest, PointerOutsideBufferFallsBackOnDeallocate)
{
  void* p = ::operator new(64, std::align_val_t(_max_align));
  _buffer.deallocate(p, 64);
}

/**
 * @brief Equality comparisons work as expected.
 *
 * Compares the same buffer instance to itself, and to different buffer
 * instances.
 */
TEST_F(BufferTest, IsEqualWorks)
{
  Buffer<64, 4> another_buffer;
  EXPECT_TRUE(_buffer == _buffer);
  EXPECT_FALSE(_buffer == another_buffer);
}

/// @} // end group PoolTests
