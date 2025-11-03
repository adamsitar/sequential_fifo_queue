#pragma once
#include "pointer_operations.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>
#include <types.h>

// Static storage for base address.
template <typename unique_tag, typename offset_type> struct thin_ptr_storage {
  inline static std::byte *_base = nullptr;
};

// Offset-based pointer with configurable offset size.
template <typename T, typename offset_type, typename unique_tag = void>
class basic_thin_ptr
    : public pointer_operations<basic_thin_ptr<T, offset_type, unique_tag>, T> {
  using storage = thin_ptr_storage<unique_tag, offset_type>;
  offset_type _offset;

  // Grant access to other instantiations for converting constructors
  template <typename, typename, typename> friend class basic_thin_ptr;

  std::byte *get_base() const {
    fatal(!storage::_base, "No base address registered for this thin_ptr!");
    return storage::_base;
  }

  // ========================================================================
  // CRTP Primitives (required by pointer_operations base)
  // ========================================================================

  T *resolve_impl() const {
    if (_offset == std::numeric_limits<offset_type>::max()) {
      return nullptr;
    }

    std::byte *base = get_base();

    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<T *>(base + _offset);
    } else {
      return std::launder(reinterpret_cast<T *>(base + _offset));
    }
  }

  bool is_null_impl() const {
    return _offset == std::numeric_limits<offset_type>::max();
  }

  void advance_impl(std::ptrdiff_t bytes) { _offset += bytes; }
  void set_null_impl() { _offset = std::numeric_limits<offset_type>::max(); }
  offset_type comparison_key() const { return _offset; }
  friend class pointer_operations<basic_thin_ptr<T, offset_type, unique_tag>,
                                  T>;

public:
  static void set_base(void *base) noexcept {
    storage::_base = static_cast<std::byte *>(base);
  }

  static void *get_base_static() noexcept { return storage::_base; }

  basic_thin_ptr() = default;
  constexpr basic_thin_ptr(std::nullptr_t) noexcept { set_null_impl(); }

  basic_thin_ptr(T *ptr) {
    if (ptr == nullptr) {
      set_null_impl();
    } else {
      auto base = reinterpret_cast<std::uintptr_t>(get_base());
      auto byte_ptr = reinterpret_cast<std::uintptr_t>(ptr);
      fatal(byte_ptr < base, "Pointer is before base");

      _offset = static_cast<offset_type>(byte_ptr - base);
      fatal(_offset == std::numeric_limits<offset_type>::max(),
            "Pointer offset collides with null sentinel value");
    }
  }

  basic_thin_ptr(void *ptr)
    requires(!std::is_void_v<T>)
  {
    if (ptr == nullptr) {
      set_null_impl();
    } else {
      std::byte *base = get_base();
      auto *byte_ptr = static_cast<std::byte *>(ptr);
      fatal(byte_ptr >= base, "Pointer is before base");

      std::ptrdiff_t byte_offset = byte_ptr - base;
      fatal(byte_offset % sizeof(T) == 0,
            "Pointer must be aligned to block boundary (cannot point inside a "
            "block)");

      _offset = static_cast<offset_type>(byte_offset);
      fatal(_offset != std::numeric_limits<offset_type>::max(),
            "Pointer offset collides with null sentinel value");
    }
  }

  offset_type offset() const noexcept { return _offset; }
};

template <typename T, provides_offset allocator>
class thin_ptr : public basic_thin_ptr<T, typename allocator::offset_type,
                                       typename allocator::unique_tag> {
  using base = basic_thin_ptr<T, typename allocator::offset_type,
                              typename allocator::unique_tag>;

public:
  // ========================================================================
  // Allocator-Specific Convenience Methods
  // ========================================================================

  /**
   * @brief Set base from allocator instance (convenience wrapper).
   * Extracts base address from allocator and sets it for all thin_ptrs with
   * this allocator's unique_tag.
   */
  static void register_allocator(allocator *alloc) noexcept {
    if (alloc) {
      base::set_base(alloc->base());
    } else {
      base::set_base(nullptr);
    }
  }

  using base::base;
};

template <typename T, typename allocator> struct pointer_for {
  using type = std::conditional_t<provides_offset<allocator>,
                                  thin_ptr<T, allocator>, T *>;
};

template <typename T, typename allocator>
using pointer_for_t = typename pointer_for<T, allocator>::type;
