#pragma once
#include <cassert>
#include <cstddef>
#include <cstring>
#include <freelist.h>
#include <memory_resource>
#include <pointers/thin_ptr.h>
#include <print>
#include <result/result.h>
#include <types.h>

// Manages a fixed number of fixed-size memory blocks in a local array/freelist.
// Always uses thin pointers for type safety and memory efficiency.
template <size_t block_size_t, size_t block_count_t, typename tag>
  requires nonzero_power_of_two<block_size_t, block_count_t>
class unique_local_buffer : public std::pmr::memory_resource {
public:
  static constexpr size_t block_size = block_size_t;
  static constexpr size_t block_align = block_size_t;
  static constexpr size_t max_block_count = block_count_t;
  static constexpr size_t total_size = block_size_t * block_count_t;

private:
  using freelist_type = freelist<block_size, block_count_t, tag>;
  freelist_type _list{};
  static std::pmr::memory_resource *_upstream;

public:
  using unique_tag = tag;
  using block_type = freelist_type::block_type;
  using offset_type = freelist_type::offset_type;
  using pointer_type = basic_thin_ptr<block_type, block_type, offset_type, tag>;

  result<pointer_type> allocate_block() {
    auto &block = ok(_list.pop());
    return pointer_type(&block);
  }

  result<> deallocate_block(pointer_type ptr) {
    fail(ptr == nullptr);

    void *raw = static_cast<void *>(ptr);
    auto *block_ref = static_cast<block_type *>(raw);

    auto res = _list.push(*block_ref);
    if (!res) {
      // Block doesn't belong to this allocator, return to upstream
      if (_upstream != nullptr) {
        _upstream->deallocate(raw, block_size, block_align);
      }
    }
    return {};
  }

  void reset() { _list.reset(); }
  std::size_t size() const noexcept { return _list.size(); };
  std::byte *base() const noexcept { return _list.base(); }

  unique_local_buffer() { pointer_type::set_base(base()); }
  ~unique_local_buffer() override { pointer_type::set_base(nullptr); }

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
      if (_upstream == nullptr) { return nullptr; }
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
      if (_upstream != nullptr) { _upstream->deallocate(ptr, size, alignment); }
      return;
    }

    deallocate_block(ptr);
  }

  bool
  do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }
};

template <size_t block_size, size_t block_count, typename tag>
  requires nonzero_power_of_two<block_size, block_count>
std::pmr::memory_resource
    *unique_local_buffer<block_size, block_count, tag>::_upstream = nullptr;

#define local_buffer(block_size, block_count)                                  \
  unique_local_buffer<block_size, block_count, decltype([] {})>

static_assert(homogenous<local_buffer(256, 8)>,
              "local_buffer must implement homogeneous_allocator concept");
static_assert(provides_offset<local_buffer(256, 8)>,
              "local_buffer must provide offset-based addressing");
