#pragma once
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <freelist.h>
#include <result/result.h>
#include <types.h>

// Non-unique, reusable component that manages a fixed number of segments.
template <size_t block_size_v, is_homogenous upstream_t>
  requires is_power_of_two<block_size_v>
class segment_manager {
public:
  static constexpr size_t blocks_per_segment =
      upstream_t::block_size / block_size_v;

  static_assert(upstream_t::block_size >= block_size_v,
                "Upstream block size must be >= requested block size");
  static_assert(
      upstream_t::block_size % block_size_v == 0,
      "Upstream block size must be a multiple of requested block size");
  static_assert(blocks_per_segment > 0,
                "At least one block must fit in upstream block");

private:
  using freelist_type = freelist_storage<block_size_v, blocks_per_segment>;
  using freelist_offset_type = typename freelist_type::offset_type;

public:
  using block_type = typename freelist_type::block_type;

private:
  struct segment_metadata {
    typename upstream_t::pointer_type segment_ptr{nullptr};
    freelist_offset_type freelist_head{
        std::numeric_limits<freelist_offset_type>::max()};
    freelist_offset_type freelist_count{0};

    bool is_valid() const noexcept { return segment_ptr != nullptr; }
    bool is_empty() const noexcept { return freelist_count == 0; }
    bool is_full() const noexcept {
      return freelist_count >= blocks_per_segment;
    }

    bool owns_block(block_type *block) const noexcept {
      if (!is_valid()) { return false; }

      auto *freelist = reinterpret_cast<const freelist_type *>(
          static_cast<const void *>(segment_ptr));
      return freelist->owns(*block);
    }

    block_type *try_allocate() noexcept {
      if (!is_valid() || is_empty()) { return nullptr; }

      auto freelist =
          reinterpret_cast<freelist_type *>(static_cast<void *>(segment_ptr));
      return static_cast<block_type *>(
          to_nullptr(freelist->pop(freelist_head, freelist_count)));
    }

    result<> deallocate(block_type *block, upstream_t *upstream) noexcept {
      auto *freelist =
          reinterpret_cast<freelist_type *>(static_cast<void *>(segment_ptr));
      ok(freelist->push(*block, freelist_head, freelist_count));

      if (is_full()) {
        ok(upstream->deallocate_block(segment_ptr));
        segment_ptr = nullptr;
      }
      return {};
    }
  };

public:
  static constexpr size_t block_size = block_size_v;
  static constexpr size_t block_align = block_size_v;
  // reserve for high_water_mark(1) + next pointer (1-2) + padding (0)
  static constexpr size_t reserve = 4;
  static constexpr size_t max_segments =
      (upstream_t::block_size - reserve) / sizeof(segment_metadata);
  static_assert(max_segments > 0,
                "Upstream block size too small for segment_manager");
  static constexpr size_t max_block_count = blocks_per_segment * max_segments;
  static constexpr size_t total_size_v = block_size * max_block_count;

  // segment count, never decreased (one past the last valid index)
  smallest_t<max_segments> _high_water_mark{0};
  std::array<segment_metadata, max_segments> _segments{};

  segment_manager() = default;
  ~segment_manager() = default;

  void cleanup(upstream_t *upstream) noexcept {
    for (auto &segment : std::span(_segments.data(), _high_water_mark)) {
      if (segment.is_valid()) {
        unwrap(upstream->deallocate_block(segment.segment_ptr));
        segment.segment_ptr = nullptr; // Mark as invalid to prevent double-free
      }
    }
  }

  // Reset all segments to initial state
  void reset(upstream_t *upstream) noexcept {
    cleanup(upstream);
    _high_water_mark = 0;
    _segments = {};
  }

  // Count total available blocks across all segments
  size_t available_count() const noexcept {
    size_t total = 0;
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].is_valid()) { total += _segments[i].freelist_count; }
    }
    return total;
  }

  segment_manager(const segment_manager &) = delete;
  segment_manager &operator=(const segment_manager &) = delete;
  segment_manager(segment_manager &&) = delete;
  segment_manager &operator=(segment_manager &&) = delete;

  result<block_type *> try_allocate(upstream_t *upstream) noexcept {
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (auto *block = _segments[i].try_allocate()) { return block; }
    }

    return allocate_new_segment(upstream);
  }

  result<> deallocate(block_type *block, upstream_t *upstream) noexcept {
    fail(block == nullptr, "cannot deallocate null block");

    auto segment_id_result =
        find_segment_for_pointer(reinterpret_cast<std::byte *>(block));
    fail(!segment_id_result, "block not owned by this manager");

    size_t segment_id = *segment_id_result;
    fail(segment_id >= _high_water_mark, "invalid segment id");

    auto &metadata = _segments[segment_id];
    fail(!metadata.is_valid(), "invalid segment");

    ok(metadata.deallocate(block, upstream));
    return {};
  }

  bool owns(block_type *block) const noexcept {
    if (block == nullptr) { return false; }

    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].owns_block(block)) { return true; }
    }

    return false;
  }

  bool has_capacity() const noexcept {
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].is_valid() && !_segments[i].is_full()) { return true; }
    }

    return _high_water_mark < max_segments;
  }

  bool is_empty() const noexcept {
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].is_valid() && !_segments[i].is_empty()) { return false; }
    }
    return true;
  }

  // O(n)
  size_t segment_count() const noexcept {
    size_t count = 0;
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].is_valid()) { ++count; }
    }
    return count;
  }

  // for pointer resolution by growing_pool
  result<std::byte *> get_segment_base(uint8_t segment_id) const noexcept {
    fail(segment_id >= _high_water_mark, "invalid segment id");
    auto &metadata = _segments[segment_id];
    fail(!metadata.is_valid(), "segment not valid");

    return static_cast<std::byte *>(static_cast<void *>(metadata.segment_ptr));
  }

  result<size_t> find_segment_for_pointer(std::byte *ptr) const noexcept {
    auto *block = reinterpret_cast<block_type *>(ptr);
    for (size_t i = 0; i < _high_water_mark; ++i) {
      if (_segments[i].is_valid() && _segments[i].owns_block(block)) {
        return i;
      }
    }

    return "pointer not owned by manager";
  }

private:
  result<size_t> find_free_slot() const noexcept {
    for (size_t i = 0; i < max_segments; ++i) {
      if (!_segments[i].is_valid()) { return i; }
    }
    fail("free slot not found").silent();
    return {};
  }

  result<block_type *> allocate_new_segment(upstream_t *upstream) noexcept {
    size_t slot = ok(find_free_slot());
    if (slot >= _high_water_mark) { _high_water_mark = slot + 1; }

    auto upstream_block = ok(upstream->allocate_block());
    typename upstream_t::pointer_type upstream_ptr = upstream_block;
    void *placement_ptr = static_cast<void *>(upstream_ptr);

    auto *freelist = new (placement_ptr) freelist_type(
        _segments[slot].freelist_head, _segments[slot].freelist_count);

    _segments[slot].segment_ptr = upstream_ptr;

    return try_allocate(upstream);
  }
};
