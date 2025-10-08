#pragma once
#include <cassert>
#include <cstddef>
#include <cstring>
#include <freelist.h>
#include <managed.h>
#include <memory_resource>
#include <new>
#include <types.h>

namespace mbb {
/**
 * @defgroup buffer Buffer
 * @brief Manages a fixed number of fixed-size memory blocks in a freelist.
 */

/**
 * @defgroup api API
 * @ingroup buffer
 */

/// @addtogroup api
/// @{

template <size_t block_size, size_t block_count>
concept BufferConcept = requires {
  (block_size > 0);
  (block_count > 0);
  is_power_of_two<block_size>;
};

/**
 * @brief PMR-compliant pool allocator.
 * Manages a fixed number of fixed-size memory blocks in a local array/freelist.
 *
 * @tparam block_size The size of each block in bytes.
 * @tparam block_count The total number of blocks in the pool.
 */
template <size_t block_size, size_t block_count>
  requires BufferConcept<block_size, block_count>
class Buffer : public Managed {
  using Block = Block<block_size>;

  std::pmr::memory_resource *_upstream{std::pmr::get_default_resource()};
  Freelist<block_size, block_count> _list{};

public:
  Buffer() = default;
  explicit Buffer(std::pmr::memory_resource *upstream) : _upstream(upstream) {}

private:
  /**
   * @brief Removes a block from the head of the freelist.
   * @time 1
   * @param bytes is > 0
   * @param alignment is > 0, power of two, <= bytes
   * @throws std::bad_alloc if null upstream
   */
  void *do_allocate(size_t bytes = block_size,
                    size_t alignment = alignof(max_align_t)) override {
    assert(bytes > 0);
    assert(alignment > 0);
    assert(alignment <= bytes);

    auto fits_within_a_block = bytes <= block_size;
    auto can_be_aligned = alignment <= block_size;
    if (!fits_within_a_block || !can_be_aligned) {
      if (_upstream == nullptr) {
        throw std::bad_alloc();
      }

      return _upstream->allocate(bytes, alignment);
    }

    return _list.pop();
  }

  /**
   * @brief Deallocates by putting the passed in block at the head of the list.
   * @time 1
   * @param ptr is not null
   * @param bytes is > 0
   * @param alignment is > 0, <= bytes
   */
  void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
    assert(ptr != nullptr);
    assert(bytes > 0);
    assert(alignment > 0);
    assert(alignment <= bytes);

    auto block = static_cast<Block *>(ptr);

    try {
      _list.push(*block);
    } catch (std::exception &e) {
      _upstream->deallocate(ptr, bytes, alignment);
    };
  }

  /**
   * @brief Performs strict pointer comparison
   * @time 1
   */
  [[nodiscard]] bool
  do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }

  /**
   * @brief Restores initial (empty) state
   * @time n
   */
  void reset() override { _list.reset(); }

  [[nodiscard]] std::size_t size() const noexcept override {
    return _list.size();
  };
};

/// @} end group api

} // namespace mbb
