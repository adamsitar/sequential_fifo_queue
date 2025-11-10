#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <pointers/growing_pool_storage.h>
#include <pointers/pointer_operations.h>
#include <result/result.h>
#include <tuple>
#include <type_traits>
#include <types.h>

// Bit-packed pointer for growing_pool allocator.
template <typename T, size_t offset_count_v, size_t segment_count_v,
          size_t manager_count_v, typename unique_tag>
class basic_growing_pool_ptr : public pointer_operations<T> {
  using storage = growing_pool_storage<unique_tag>;

public:
  // Calculate bits needed to represent indices (count - 1)
  static constexpr size_t offset_bits = std::bit_width(offset_count_v - 1);
  static constexpr size_t segment_bits = std::bit_width(segment_count_v - 1);
  static constexpr size_t manager_bits = std::bit_width(manager_count_v - 1);
  static constexpr size_t total_bits =
      offset_bits + segment_bits + manager_bits;

  // Null sentinel: reserve the maximum value representable in manager_bits
  static constexpr size_t null_manager_index = (1ULL << manager_bits) - 1;

  // Maximum valid indices
  static constexpr size_t max_offset_index = (1ULL << offset_bits) - 1;
  static constexpr size_t max_segment_index = (1ULL << segment_bits) - 1;
  static constexpr size_t max_manager_index = null_manager_index - 1;

  using id_storage_type = smallest_t<total_bits>;
  using underlying_type = id_storage_type::underlying_type;

  static_assert(total_bits <= 64, "Total bits exceeds 64-bit storage");
  static_assert(offset_bits > 0, "offset_bits must be at least 1");
  static_assert(segment_bits > 0, "segment_bits must be at least 1");
  static_assert(manager_bits > 0, "manager_bits must be at least 1");

private:
  struct {
    underlying_type offset : offset_bits;
    underlying_type segment : segment_bits;
    underlying_type manager : manager_bits;
  } _id;

  template <typename, size_t, size_t, size_t, typename>
  friend class basic_growing_pool_ptr;
  friend class pointer_operations<T>;

  T *resolve_impl() const {
    if (is_null_impl()) { return nullptr; }

    size_t manager_id = get_manager_id();
    size_t segment_id = get_segment_id();
    size_t offset = get_offset();

    return resolve_via_storage(manager_id, segment_id, offset);
  }

  void advance_impl(int bytes) {
    if (is_null_impl()) { return; }

    // Convert bytes to element count
    int elements = 0;
    if constexpr (std::is_void_v<T>) {
      elements = bytes;
    } else {
      elements = bytes / sizeof(T);
    }

    constexpr size_t blocks_per_segment = offset_count_v;
    constexpr size_t blocks_per_manager = blocks_per_segment * segment_count_v;
    // Valid managers: 0 to max_manager_index (null_manager_index is reserved)
    constexpr size_t total_blocks =
        (max_manager_index + 1) * blocks_per_manager;

    // Calculate absolute linear position across ALL managers
    std::uintptr_t current_linear =
        (static_cast<std::uintptr_t>(_id.manager) * blocks_per_manager) +
        (static_cast<std::uintptr_t>(_id.segment) * blocks_per_segment) +
        static_cast<std::uintptr_t>(_id.offset);

    std::uintptr_t new_linear = current_linear + elements;

    // Underflow detection: wrapping when adding negative elements
    if (elements < 0) {
      fatal(new_linear > current_linear,
            "pointer arithmetic underflow - before start of pool");
    }

    // Overflow check: exceeds total pool capacity
    fatal(new_linear >= total_blocks,
          "pointer arithmetic overflow - beyond end of pool");

    // Decompose absolute position back to (manager, segment, offset)
    _id.manager = static_cast<underlying_type>(new_linear / blocks_per_manager);
    std::uintptr_t within_manager = new_linear % blocks_per_manager;
    _id.segment =
        static_cast<underlying_type>(within_manager / blocks_per_segment);
    _id.offset =
        static_cast<underlying_type>(within_manager % blocks_per_segment);
  }

  bool is_null_impl() const { return _id.manager == null_manager_index; }

  constexpr void set_null_impl() {
    _id.manager = static_cast<underlying_type>(null_manager_index);
  }

  auto comparison_key() const {
    // Lexicographic comparison: null sorts first (null < valid), then manager,
    // segment, offset
    // For null pointers, use false as first element (false < true ensures null
    // < valid)
    // Lexicographic comparison stops at first difference, so when comparing
    // null to valid, only the bool is examined; the ID values are ignored
    if (is_null_impl()) {
      return std::make_tuple(false, size_t{0}, size_t{0}, size_t{0});
    }
    return std::make_tuple(true, get_manager_id(), get_segment_id(),
                           get_offset());
  }

  // Helper for resolution (needs to be specialized based on block_size)
  // This will be called with the unpacked IDs
  template <typename manager_type>
  T *resolve_with_manager(manager_type *manager, size_t segment_id,
                          size_t offset) const {
    auto segment_base_result = manager->get_segment_base(segment_id);
    if (!segment_base_result) { return nullptr; }

    std::byte *segment_base = *segment_base_result;

    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<T *>(segment_base + offset);
    } else {
      // offset is in units of sizeof(T)
      return std::launder(
          reinterpret_cast<T *>(segment_base + (offset * sizeof(T))));
    }
  }

  // Type-erased resolution via storage
  T *resolve_via_storage(size_t manager_id, size_t segment_id,
                         size_t offset) const {
    auto resolve_result = storage::template resolve_pointer<T>(
        manager_id, segment_id, offset, sizeof(T));
    return resolve_result ? *resolve_result : nullptr;
  }

public:
  // Rebind this pointer type to a different pointed-to type
  template <typename U>
  using rebind = basic_growing_pool_ptr<U, offset_count_v, segment_count_v,
                                        manager_count_v, unique_tag>;

  constexpr basic_growing_pool_ptr() { set_null_impl(); }
  constexpr basic_growing_pool_ptr(std::nullptr_t) { set_null_impl(); }

  basic_growing_pool_ptr(size_t manager_id, size_t segment_id, size_t offset) {
    // Max valid values: a field with N bits can store 0 to (2^N - 1)

    fatal(manager_id > max_manager_index, "manager_id out of range or null");
    fatal(segment_id > max_segment_index, "segment_id out of range");
    fatal(offset > max_offset_index, "offset out of range");

    _id.manager = static_cast<underlying_type>(manager_id);
    _id.segment = static_cast<underlying_type>(segment_id);
    _id.offset = static_cast<underlying_type>(offset);
  }

  explicit basic_growing_pool_ptr(void *ptr) {
    if (ptr == nullptr) {
      set_null_impl();
      return;
    }

    // Find which manager owns this pointer
    auto manager_id_result =
        storage::find_manager_for_pointer(static_cast<std::byte *>(ptr));
    if (!manager_id_result) {
      set_null_impl();
      return;
    }

    size_t manager_id = *manager_id_result;

    // Get the manager and find which segment owns the pointer
    // This requires calling back to storage
    auto segment_id_result = storage::find_segment_in_manager(
        manager_id, static_cast<std::byte *>(ptr));
    if (!segment_id_result) {
      set_null_impl();
      return;
    }

    size_t segment_id = *segment_id_result;

    // Compute offset within segment
    auto offset_result = storage::compute_offset_in_segment(
        manager_id, segment_id, static_cast<std::byte *>(ptr), sizeof(T),
        std::is_void_v<T>);
    if (!offset_result) {
      set_null_impl();
      return;
    }

    *this = basic_growing_pool_ptr(manager_id, segment_id, *offset_result);
  }

  explicit basic_growing_pool_ptr(T *ptr)
    requires(!std::is_void_v<T>)
      : basic_growing_pool_ptr(static_cast<void *>(ptr)) {}

  template <typename U>
    requires(!std::same_as<T, U>)
  basic_growing_pool_ptr(
      const basic_growing_pool_ptr<U, offset_count_v, segment_count_v,
                                   manager_count_v, unique_tag> &other)
      : basic_growing_pool_ptr(static_cast<void *>(other.resolve_impl())) {}

  template <typename U>
    requires(!std::same_as<T, U>)
  basic_growing_pool_ptr &
  operator=(const basic_growing_pool_ptr<U, offset_count_v, segment_count_v,
                                         manager_count_v, unique_tag> &other) {
    *this = basic_growing_pool_ptr(static_cast<void *>(other.resolve_impl()));
    return *this;
  }

  size_t get_manager_id() const {
    fatal(is_null_impl(), "cannot get manager_id from null pointer");
    return _id.manager;
  }
  size_t get_segment_id() const {
    fatal(is_null_impl(), "cannot get segment_id from null pointer");
    return _id.segment;
  }
  size_t get_offset() const {
    fatal(is_null_impl(), "cannot get offset from null pointer");
    return _id.offset;
  }

  template <typename U = T>
    requires(!std::is_void_v<U> && std::same_as<U, T>)
  static basic_growing_pool_ptr pointer_to(U &r) {
    return basic_growing_pool_ptr(std::addressof(r));
  }

  static constexpr size_t storage_bits() { return total_bits; }
  static constexpr size_t storage_bytes() { return sizeof(id_storage_type); }
};
