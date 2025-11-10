#pragma once
#include <cassert>
#include <core/ptr_utils.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <pointers/pointer_operations.h>
#include <print>
#include <type_traits>
#include <types.h>

// Static storage for base address.
template <typename unique_tag, typename offset_type> struct thin_ptr_storage {
  inline static std::byte *_base = nullptr;
};

// Offset-based pointer with configurable offset size.
template <typename T, typename offset_type, typename unique_tag = void>
class basic_thin_ptr : public pointer_operations<T> {
  using storage = thin_ptr_storage<unique_tag, offset_type>;
  offset_type _offset;

  template <typename, typename, typename> friend class basic_thin_ptr;
  friend class pointer_operations<T>;

  std::byte *get_base() const {
    fatal(!storage::_base, "No base address registered for this thin_ptr!");
    return storage::_base;
  }

  T *resolve_impl() const {
    if (_offset == offset_type::null_sentinel) { return nullptr; }
    std::byte *base = get_base();

    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<T *>(base + _offset);
    } else {
      return std::launder(reinterpret_cast<T *>(base + _offset * sizeof(T)));
    }
  }

  bool is_null_impl() const { return _offset == offset_type::null_sentinel; }
  void advance_impl(std::ptrdiff_t bytes) { _offset += bytes; }
  void set_null_impl() { _offset = offset_type::null_sentinel; }
  offset_type comparison_key() const { return _offset; }

public:
  // rebind pointer type to a different pointed-to type
  template <typename U>
  using rebind = basic_thin_ptr<U, offset_type, unique_tag>;

  static void set_base(void *base) {
    storage::_base = static_cast<std::byte *>(base);
  }

  static void *get_base_static() { return storage::_base; }
  basic_thin_ptr() = default;
  constexpr basic_thin_ptr(std::nullptr_t) { set_null_impl(); }

  basic_thin_ptr(T *ptr) {
    if (ptr == nullptr) {
      set_null_impl();
    } else {
      auto *base = get_base();
      auto byte_offset = ptr::offset(base, ptr);
      _offset = byte_offset / sizeof(T);
      fatal(_offset == offset_type::null_sentinel,
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
      auto *typed_ptr = static_cast<T *>(ptr);
      _offset = ptr::element_index(base, typed_ptr);
      fatal(_offset == offset_type::null_sentinel,
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
