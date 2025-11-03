#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
#include <pointer_operations.h>
#include <print>
#include <result.h>
#include <tuple>
#include <type_traits>
#include <types.h>

#define NULL_SENTINEL std::numeric_limits<offset_type>::max()

// Stores a pointer to the allocator instance for segment base resolution.
// Type erased to avoid circular dependencies between storage and buffer.
template <typename unique_tag> struct segmented_ptr_storage {
  inline static void *_buffer_instance;
  inline static result<std::byte *> (*_lookup_fn)(void *, uint8_t);
  inline static result<size_t> (*_find_segment_fn)(void *, std::byte *);

  template <typename buffer_type>
  static result<> register_buffer(buffer_type *buffer) {
    fail(buffer == nullptr);
    fail(_buffer_instance != nullptr, "buffer already registered");

    _buffer_instance = buffer;

    _lookup_fn = +[](void *buf, uint8_t segment_id) -> result<std::byte *> {
      auto *typed_buf = static_cast<buffer_type *>(buf);
      return TRY(typed_buf->get_segment_base(segment_id));
    };

    _find_segment_fn = +[](void *buf, std::byte *ptr) -> result<size_t> {
      auto *typed_buf = static_cast<buffer_type *>(buf);
      return TRY(typed_buf->find_segment_for_pointer(ptr));
    };
    return {};
  }

  static void unregister_buffer() noexcept {
    _buffer_instance = nullptr;
    _lookup_fn = nullptr;
    _find_segment_fn = nullptr;
  }

  // Delegates to the registered buffer instance via type-erased lookup.
  static result<std::byte *> get_segment_base(auto segment_id) {
    fail(_lookup_fn == nullptr, "buffer not registered");
    return TRY(_lookup_fn(_buffer_instance, static_cast<uint8_t>(segment_id)));
  }

  // Performs O(n) search through segments. Used by converting constructor.
  static result<size_t> find_segment_for_pointer(std::byte *ptr) {
    fail(_find_segment_fn == nullptr, "buffer not registered");
    return TRY(_find_segment_fn(_buffer_instance, ptr));
  }
};

template <typename T, typename offset_type, typename unique_tag,
          typename segment_id_type = uint8_t>
class basic_segmented_ptr
    : public pointer_operations<
          basic_segmented_ptr<T, offset_type, unique_tag, segment_id_type>, T> {
  using storage = segmented_ptr_storage<unique_tag>;

  segment_id_type _segment_id;
  offset_type _offset;

  template <typename, typename, typename, typename>
  friend class basic_segmented_ptr;

  friend class pointer_operations<
      basic_segmented_ptr<T, offset_type, unique_tag, segment_id_type>, T>;

  // ========================================================================
  // CRTP Primitives
  // ========================================================================

  T *resolve_impl() const {
    if (_offset == NULL_SENTINEL) {
      return nullptr;
    }

    std::byte *base = unwrap(storage::get_segment_base(_segment_id));

    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<T *>(base + _offset);
    } else {
      return std::launder(reinterpret_cast<T *>(base + (_offset * sizeof(T))));
    }
  }

  void advance_impl(std::ptrdiff_t bytes) { _offset += bytes; }

  bool is_null_impl() const { return _offset == NULL_SENTINEL; }

  void set_null_impl() noexcept {
    _segment_id = 0;
    _offset = NULL_SENTINEL;
  }

  auto comparison_key() const {
    // Return tuple for lexicographic comparison: first by segment, then offset
    return std::make_tuple(_segment_id, _offset);
  }

public:
  // ========================================================================
  // Constructors
  // ========================================================================

  basic_segmented_ptr() = default;

  constexpr basic_segmented_ptr(std::nullptr_t) noexcept { set_null_impl(); }

  // Construct from segment ID and raw pointer.
  basic_segmented_ptr(segment_id_type segment_id, T *ptr)
      : _segment_id(segment_id) {
    if (ptr == nullptr) {
      set_null_impl();
    } else {
      std::byte *base = unwrap(storage::get_segment_base(segment_id));
      auto *byte_ptr = reinterpret_cast<std::byte *>(ptr);
      // std::println("{} -  {}", (void *)byte_ptr, (void *)base);
      fatal(byte_ptr < base, "Pointer is before segment base");

      if constexpr (std::is_void_v<T>) {
        // For void*, store byte offset
        _offset = static_cast<offset_type>(byte_ptr - base);
      } else {
        // For typed pointers, compute block index
        auto byte_offset = byte_ptr - base;
        auto block_index = byte_offset / sizeof(T);
        _offset = static_cast<offset_type>(block_index);
      }

      fatal(_offset == std::numeric_limits<offset_type>::max(),
            "Pointer offset collides with null sentinel value");
    }
  }

  basic_segmented_ptr(segment_id_type segment_id, offset_type offset)
      : _segment_id(segment_id), _offset(offset) {}

  // Converting constructor from typed pointer (delegates to void*)
  basic_segmented_ptr(T *ptr)
    requires(!std::is_void_v<T>)
      : basic_segmented_ptr(static_cast<void *>(ptr)) {}

  // Converting constructor from void pointer
  // Scans all segments to find which one owns the pointer, O(n).
  basic_segmented_ptr(void *ptr) {
    if (ptr == nullptr) {
      set_null_impl();
      return;
    }

    auto *byte_ptr = static_cast<std::byte *>(ptr);
    auto segment_id_result = storage::find_segment_for_pointer(byte_ptr);
    if (!segment_id_result) {
      // Pointer doesn't belong to this buffer, return null
      set_null_impl();
      return;
    }
    auto segment_id = *segment_id_result;
    _segment_id = static_cast<segment_id_type>(segment_id);

    auto *base = unwrap(storage::get_segment_base(segment_id));

    if constexpr (std::is_void_v<T>) {
      // For void*, store byte offset
      _offset = static_cast<offset_type>(byte_ptr - base);
    } else {
      // For typed pointers, compute block index and verify alignment
      auto byte_offset = byte_ptr - base;
      fatal(!(byte_offset % sizeof(T) == 0),
            "Pointer must be aligned to block boundary (cannot point inside a "
            "block)");

      auto block_index = byte_offset / sizeof(T);
      _offset = static_cast<offset_type>(block_index);
    }

    fatal(_offset == std::numeric_limits<offset_type>::max(),
          "Pointer offset collides with null sentinel value");
  }

  // Converting constructor from segmented_ptr with different pointed-to
  template <typename U>
    requires(!std::same_as<T, U> && sizeof(T) == sizeof(U))
  basic_segmented_ptr(
      const basic_segmented_ptr<U, offset_type, unique_tag, segment_id_type>
          &other) noexcept
      : _segment_id(other._segment_id), _offset(other._offset) {}

  // Converting assignment from segmented_ptr with different pointed-to
  template <typename U>
    requires(!std::same_as<T, U> && sizeof(T) == sizeof(U))
  basic_segmented_ptr &
  operator=(const basic_segmented_ptr<U, offset_type, unique_tag,
                                      segment_id_type> &other) noexcept {
    _segment_id = other._segment_id;
    _offset = other._offset;
    return *this;
  }

  segment_id_type get_segment_id() const noexcept { return _segment_id; }
  offset_type get_offset() const noexcept { return _offset; }

  // ========================================================================
  // pointer_traits support (required by std::forward_list and other STL)
  // ========================================================================

  /**
   * @brief Create a segmented_ptr from a reference (required by
   * pointer_traits).
   *
   * This enables use with standard containers that use fancy pointers.
   * Uses the existing void* constructor which performs O(n) segment search.
   */
  template <typename U = T>
    requires(!std::is_void_v<U> && std::same_as<U, T>)
  static basic_segmented_ptr pointer_to(U &r) noexcept {
    return basic_segmented_ptr(std::addressof(r));
  }
};

template <typename T, typename allocator>
class segmented_ptr
    : public basic_segmented_ptr<T, typename allocator::offset_type,
                                 typename allocator::unique_tag, uint8_t> {
  using base = basic_segmented_ptr<T, typename allocator::offset_type,
                                   typename allocator::unique_tag, uint8_t>;

public:
  using base::base;
};
