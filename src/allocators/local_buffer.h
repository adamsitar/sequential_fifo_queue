#pragma once
#include "result.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <freelist.h>
#include <memory_resource>
#include <thin_ptr.h>
#include <types.h>

// Manages a fixed number of fixed-size memory blocks in a local array/freelist.
template <size_t block_size, size_t block_count, typename tag = void>
  requires nonzero_power_of_two<block_size, block_count>
class unique_local_buffer : public std::pmr::memory_resource {
public:
  static constexpr size_t block_size_v = block_size;
  static constexpr size_t block_align_v = block_size;
  static constexpr size_t block_count_v = block_count;
  static constexpr size_t total_size_v = block_size * block_count;

private:
  using freelist_type = freelist<block_size, block_count, tag>;
  freelist_type _list{};
  static std::pmr::memory_resource *_upstream;

public:
  using unique_tag = tag;
  using block_type = freelist_type::block_type;
  using offset_type = freelist_type::offset_type;

  using pointer_type = std::conditional_t<
      std::is_void_v<tag>, void *,
      basic_thin_ptr<typename freelist_type::block_type, offset_type, tag>>;

  // ============================================================================
  // Homogeneous Allocator Interface
  // ============================================================================

  result<pointer_type> allocate_block() {
    auto raw_block = TRY(_list.pop());
    if constexpr (std::is_void_v<tag>) {
      return static_cast<void *>(raw_block);
    } else {
      return pointer_type(raw_block);
    }
  }

  result<> deallocate_block(pointer_type ptr) {
    fail(ptr == nullptr);

    void *raw = static_cast<void *>(ptr);
    auto *block_ref = static_cast<block_type *>(raw);

    auto res = _list.push(*block_ref);
    if (!res) {
      // Block doesn't belong to this allocator, return to upstream
      if (_upstream != nullptr) {
        _upstream->deallocate(raw, block_size_v, block_align_v);
      }
    }
    return {};
  }

  void reset() { _list.reset(); }

  [[nodiscard]] std::size_t size() const noexcept { return _list.size(); };

  [[nodiscard]] std::byte *base() const noexcept {
    return const_cast<std::byte *>(_list.base());
  }

  // ============================================================================
  // Standard Interface
  // ============================================================================

  unique_local_buffer() {
    if constexpr (!std::is_void_v<tag>) {
      pointer_type::set_base(base());
    }
  }

  ~unique_local_buffer() override {
    if constexpr (!std::is_void_v<tag>) {
      pointer_type::set_base(nullptr);
    }
  }

  static void set_upstream(std::pmr::memory_resource *upstream) noexcept {
    _upstream = upstream;
  }

  static std::pmr::memory_resource *get_upstream() noexcept {
    return _upstream;
  }

private:
  void *do_allocate(size_t size, size_t alignment) override {
    fatal(size == 0);
    fatal(alignment == 0);
    fatal(alignment > size, "alignment cannot exceed size");

    auto fits_within_a_block = size <= block_size;
    auto can_be_aligned = alignment <= block_size;
    if (!fits_within_a_block || !can_be_aligned) {
      if (_upstream == nullptr) {
        return nullptr;
      }
      return _upstream->allocate(size, alignment);
    }

    return to_nullptr(allocate_block());
  }

  void do_deallocate(void *ptr, size_t size, size_t alignment) override {
    fatal(ptr == nullptr);
    fatal(size == 0);
    fatal(alignment == 0);
    fatal(alignment > size, "alignment cannot exceed size");

    auto fits_within_a_block = size <= block_size;
    auto can_be_aligned = alignment <= block_size;
    if (!fits_within_a_block || !can_be_aligned) {
      if (_upstream != nullptr) {
        _upstream->deallocate(ptr, size, alignment);
      }
      return;
    }

    // Convert void* to pointer_type and delegate to primary interface
    if constexpr (std::is_void_v<tag>) {
      unwrap(deallocate_block(ptr));
    } else {
      unwrap(deallocate_block(pointer_type(ptr)));
    }
  }

  [[nodiscard]] bool
  do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }
};

template <size_t BlockSize, size_t NumBlocks, typename Tag>
  requires nonzero_power_of_two<BlockSize, NumBlocks>
std::pmr::memory_resource
    *unique_local_buffer<BlockSize, NumBlocks, Tag>::_upstream = nullptr;

#define local_buffer(block_size, block_count)                                  \
  unique_local_buffer<block_size, block_count, decltype([] {})>

static_assert(homogenous<local_buffer(256, 8)>,
              "local_buffer must implement homogeneous_allocator concept");

static_assert(provides_offset<local_buffer(256, 8)>,
              "local_buffer must provide offset-based addressing");
