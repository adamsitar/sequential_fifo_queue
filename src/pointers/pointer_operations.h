#pragma once
#include <cassert>
#include <compare>
#include <cstddef>
#include <type_traits>

template <typename T> class pointer_operations {
public:
  auto &operator*(this auto const &self)
    requires(!std::is_void_v<T>)
  {
    T *ptr = self.resolve_impl();
    fatal(ptr, "Dereferencing null pointer!");
    return *ptr;
  }

  T *operator->(this auto const &self) {
    T *ptr = self.resolve_impl();
    fatal(ptr == nullptr, "Dereferencing null pointer!");
    return ptr;
  }

  auto &operator[](this auto const &self, std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    T *ptr = self.resolve_impl();
    return ptr[n];
  }

  auto &operator++(this auto &self)
    requires(!std::is_void_v<T>)
  {
    self.advance_impl(sizeof(T));
    return self;
  }

  auto operator++(this auto &self, int)
    requires(!std::is_void_v<T>)
  {
    auto tmp = self; // Copy of derived type
    ++self;
    return tmp;
  }

  auto &operator--(this auto &self)
    requires(!std::is_void_v<T>)
  {
    self.advance_impl(-sizeof(T));
    return self;
  }

  auto operator--(this auto &self, int)
    requires(!std::is_void_v<T>)
  {
    auto tmp = self;
    --self;
    return tmp;
  }

  auto &operator+=(this auto &self, std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    self.advance_impl(n * sizeof(T));
    return self;
  }

  auto &operator-=(this auto &self, std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    self.advance_impl(-n * sizeof(T));
    return self;
  }

  auto operator+(this auto const &self, std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    auto result = self;
    result += n;
    return result;
  }

  auto operator-(this auto const &self, std::ptrdiff_t n)
    requires(!std::is_void_v<T>)
  {
    auto result = self;
    result -= n;
    return result;
  }

  template <typename other_t>
  std::ptrdiff_t operator-(this auto const &self, other_t const &other)
    requires(!std::is_void_v<T>)
  {
    T *this_ptr = self.resolve_impl();
    T *other_ptr;

    if constexpr (std::is_pointer_v<other_t>) {
      other_ptr = other;
    } else {
      other_ptr = other.resolve_impl();
    }

    return this_ptr - other_ptr;
  }

  // generates <, <=, >, >=
  template <typename other_t>
  auto operator<=>(this auto const &self, other_t const &other) {
    if constexpr (std::is_pointer_v<other_t>) {
      return self.resolve_impl() <=> other;
    } else {
      return self.comparison_key() <=> other.comparison_key();
    }
  }

  // generates != via C++20 operator rewriting
  template <typename other_t>
  bool operator==(this auto const &self, other_t const &other) {
    if constexpr (std::is_pointer_v<other_t>) {
      return self.resolve_impl() == other;
    } else {
      return self.comparison_key() == other.comparison_key();
    }
  }

  bool operator==(this auto const &self, std::nullptr_t) {
    return self.is_null_impl();
  }

  explicit operator bool(this auto const &self) noexcept {
    return !self.is_null_impl();
  }

  operator T *(this auto const &self)
    requires(!std::is_void_v<T>)
  {
    return self.resolve_impl();
  }

  operator void *(this auto const &self) {
    return static_cast<void *>(self.resolve_impl());
  }
};

template <typename T>
  requires(!std::is_void_v<T>)
auto operator+(std::ptrdiff_t n, const pointer_operations<T> &ptr) {
  return ptr + n;
}
