#pragma once
#include <cstddef>
#include <pointers/growing_pool_storage.h>
#include <pointers/pointer_operations.h>
#include <result/result.h>
#include <tuple>
#include <type_traits>
#include <types.h>

// Bit-packed pointer for growing_pool allocator.
template <typename T, size_t offset_bits, size_t segment_bits,
          size_t manager_bits, typename unique_tag>
class basic_growing_pool_ptr : public pointer_operations<T> {
  using storage = growing_pool_storage<unique_tag>;
  static constexpr size_t total_bits =
      offset_bits + segment_bits + manager_bits;

public:
  using id_storage_type = smallest_t<total_bits>;
  using underlying_type = id_storage_type::underlying_type;

private:
  static_assert(total_bits <= 64, "Total bits exceeds 64-bit storage");
  static_assert(offset_bits > 0, "offset_bits must be at least 1");
  static_assert(segment_bits > 0, "segment_bits must be at least 1");
  static_assert(manager_bits > 0, "manager_bits must be at least 1");

  // Null sentinel: reserve max manager_id value to represent null
  static constexpr size_t NULL_MANAGER = (1ULL << manager_bits) - 1;

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

    // Get the segment_manager (type-erased, so we need to know the type)
    // For now, we'll delegate to storage which knows the concrete type
    return resolve_via_storage(manager_id, segment_id, offset);
  }

  void advance_impl(std::ptrdiff_t bytes) {
    if (is_null_impl()) { return; }

    if constexpr (std::is_void_v<T>) {
      _id.offset += bytes;
    } else {
      _id.offset += bytes / sizeof(T);
    }

    // TODO: Add debug-mode bounds checking
  }

  bool is_null_impl() const { return _id.manager == NULL_MANAGER; }

  constexpr void set_null_impl() {
    _id.manager = static_cast<underlying_type>(NULL_MANAGER);
  }

  auto comparison_key() const {
    // Lexicographic comparison: first by manager, then segment, then offset
    return std::make_tuple(get_manager_id(), get_segment_id(), get_offset());
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
  using rebind = basic_growing_pool_ptr<U, offset_bits, segment_bits,
                                        manager_bits, unique_tag>;

  constexpr basic_growing_pool_ptr() { set_null_impl(); }
  constexpr basic_growing_pool_ptr(std::nullptr_t) { set_null_impl(); }

  // Construct from unpacked components
  basic_growing_pool_ptr(size_t manager_id, size_t segment_id, size_t offset) {
    assert(manager_id < NULL_MANAGER && "manager_id out of range or null");
    assert(segment_id < (1ULL << segment_bits) && "segment_id out of range");
    assert(offset < (1ULL << offset_bits) && "offset out of range");

    _id.manager = static_cast<underlying_type>(manager_id);
    _id.segment = static_cast<underlying_type>(segment_id);
    _id.offset = static_cast<underlying_type>(offset);
  }

  // Converting constructor from void pointer
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

  // Converting constructor from typed pointer
  explicit basic_growing_pool_ptr(T *ptr)
    requires(!std::is_void_v<T>)
      : basic_growing_pool_ptr(static_cast<void *>(ptr)) {}

  // Converting constructor from different pointed-to type
  template <typename U>
    requires(!std::same_as<T, U>)
  basic_growing_pool_ptr(
      const basic_growing_pool_ptr<U, offset_bits, segment_bits, manager_bits,
                                   unique_tag> &other)
      : basic_growing_pool_ptr(static_cast<void *>(other.resolve_impl())) {}

  // Converting assignment
  template <typename U>
    requires(!std::same_as<T, U>)
  basic_growing_pool_ptr &
  operator=(const basic_growing_pool_ptr<U, offset_bits, segment_bits,
                                         manager_bits, unique_tag> &other) {
    *this = basic_growing_pool_ptr(static_cast<void *>(other.resolve_impl()));
    return *this;
  }

  size_t get_manager_id() const { return is_null_impl() ? 0 : _id.manager; }
  size_t get_segment_id() const { return is_null_impl() ? 0 : _id.segment; }
  size_t get_offset() const { return is_null_impl() ? 0 : _id.offset; }

  template <typename U = T>
    requires(!std::is_void_v<U> && std::same_as<U, T>)
  static basic_growing_pool_ptr pointer_to(U &r) {
    return basic_growing_pool_ptr(std::addressof(r));
  }

  static constexpr size_t storage_bits() { return total_bits; }
  static constexpr size_t storage_bytes() { return sizeof(id_storage_type); }
};
