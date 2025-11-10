#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <print>
#include <result/result.h>
#include <type_traits>

namespace ptr {

// Safely narrow a value from one type to another, with overflow checking
template <typename to_t, typename from_t>
constexpr to_t narrow_cast(from_t value) noexcept {
  // Check for negative values when converting to unsigned
  if constexpr (std::is_signed_v<from_t> && std::is_unsigned_v<to_t>) {
    fatal(value < 0, "narrowing conversion: negative value to unsigned type");
  }

  // Check upper bound - handles both signed->unsigned and large->small
  if constexpr (sizeof(to_t) < sizeof(from_t) ||
                (std::is_signed_v<to_t> && std::is_unsigned_v<from_t>)) {
    using common_t = std::common_type_t<to_t, from_t>;
    fatal(static_cast<common_t>(value) >
              static_cast<common_t>(std::numeric_limits<to_t>::max()),
          "narrowing conversion overflow: value too large");
  }

  // Check lower bound for signed types
  if constexpr (std::is_signed_v<to_t> && std::is_signed_v<from_t>) {
    if constexpr (sizeof(to_t) < sizeof(from_t)) {
      fatal(value < std::numeric_limits<to_t>::min(),
            "narrowing conversion underflow: value too small");
    }
  }

  return static_cast<to_t>(value);
}

constexpr std::uintptr_t addr(const auto *ptr) {
  return reinterpret_cast<std::uintptr_t>(ptr);
}
constexpr std::uintptr_t addr(const auto &ref) { return addr(&ref); }

std::ptrdiff_t offset(const auto *from, const auto *to) noexcept {
  return static_cast<std::ptrdiff_t>(addr(to) - addr(from));
}
template <typename T, typename U>
  requires(!std::is_pointer_v<T> && !std::is_pointer_v<U>)
std::ptrdiff_t offset(const T &from, const U &to) {
  return offset(&from, &to);
}

// [begin, end)
constexpr bool contains(const auto *begin, const auto *end, const auto *ptr) {
  return ptr >= begin && ptr < end;
}

// ptr is in byte range starting at 'begin' with 'size_bytes' length
constexpr bool contains_bytes(const auto *begin, std::size_t size_bytes,
                              const void *ptr) {
  auto p = addr(ptr);
  auto b = addr(begin);
  return p >= b && p < (b + size_bytes);
}

constexpr std::ptrdiff_t index(const auto *ptr, const auto *base) {
  return ptr - base;
}
constexpr std::ptrdiff_t index(const auto &elem, const auto *base) {
  return index(&elem, base);
}

template <typename base_t>
constexpr std::ptrdiff_t element_index(const base_t *base,
                                       const void *ptr) noexcept {
  auto byte_offset = offset(base, ptr);
  fatal(byte_offset % sizeof(base_t) != 0,
        "pointer is not aligned to element boundary");
  return byte_offset / sizeof(base_t);
}

template <typename base_t>
constexpr std::ptrdiff_t element_index(const base_t *base,
                                       const base_t &elem) noexcept {
  return element_index(base, &elem);
}

} // namespace ptr
