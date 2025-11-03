#pragma once
#include <cassert>
#include <compare>
#include <cstddef>
#include <type_traits>

template <typename derived, typename T> class pointer_operations {
protected:
  derived &self() { return static_cast<derived &>(*this); }
  const derived &self() const { return static_cast<const derived &>(*this); }

public:
  // ========================================================================
  // Pointer Operations
  // ========================================================================

  // Dereference operators only valid when T is not void
  // Use auto& return type to defer type checking until function is instantiated
  auto &operator*() const
    requires(!std::is_void_v<T>)
  {
    T *ptr = self().resolve_impl();
    fatal(ptr, "Dereferencing null pointer!");
    return *ptr;
  }

  T *operator->() const {
    T *ptr = self().resolve_impl();
    fatal(ptr == nullptr, "Dereferencing null pointer!");
    return ptr;
  }

  auto &operator[](std::ptrdiff_t n) const
    requires(!std::is_void_v<T>)
  {
    T *ptr = self().resolve_impl();
    return ptr[n];
  }

  // ========================================================================
  // Arithmetic Operations (disabled for void*)
  // ========================================================================

  derived &operator++()
    requires(!std::is_void_v<T>)
  {
    self().advance_impl(sizeof(T));
    return self();
  }

  derived operator++(int)
    requires(!std::is_void_v<T>)
  {
    derived tmp = self();
    ++self();
    return tmp;
  }

  derived &operator--()
    requires(!std::is_void_v<T>)
  {
    self().advance_impl(-static_cast<std::ptrdiff_t>(sizeof(T)));
    return self();
  }

  derived operator--(int)
    requires(!std::is_void_v<T>)
  {
    derived tmp = self();
    --self();
    return tmp;
  }

  derived &operator+=(std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    self().advance_impl(n * sizeof(T));
    return self();
  }

  derived &operator-=(std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    self().advance_impl(-n * sizeof(T));
    return self();
  }

  derived operator+(std::ptrdiff_t n) const
    requires(!std::is_void_v<T>)
  {
    derived result = self();
    result += n;
    return result;
  }

  derived operator-(std::ptrdiff_t n) const
    requires(!std::is_void_v<T>)
  {
    derived result = self();
    result -= n;
    return result;
  }

  std::ptrdiff_t operator-(const derived &other) const
    requires(!std::is_void_v<T>)
  {
    // Pointer difference in elements
    T *this_ptr = self().resolve_impl();
    T *other_ptr = other.resolve_impl();
    return this_ptr - other_ptr;
  }

  // ========================================================================
  // Comparison Operations
  // ========================================================================

  bool operator==(const derived &other) const {
    return self().comparison_key() == other.comparison_key();
  }

  auto operator<=>(const derived &other) const {
    return self().comparison_key() <=> other.comparison_key();
  }

  bool operator==(std::nullptr_t) const { return self().is_null_impl(); }

  // ========================================================================
  // Conversions
  // ========================================================================

  explicit operator bool() const noexcept { return !self().is_null_impl(); }

  operator T *() const
    requires(!std::is_void_v<T>)
  {
    return self().resolve_impl();
  }

  operator void *() const { return static_cast<void *>(self().resolve_impl()); }
};

template <typename derived, typename T>
  requires(!std::is_void_v<T>)
derived operator+(std::ptrdiff_t n, const pointer_operations<derived, T> &ptr) {
  return static_cast<const derived &>(ptr) + n;
}
