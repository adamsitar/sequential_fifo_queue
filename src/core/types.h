#pragma once
#include <bit>
#include <concepts.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ptr_utils.h>

#define exforward(x) std::forward<decltype(x)>(x)

template <std::uint64_t N> constexpr auto smallest_underlying_type() {
  constexpr std::size_t numBits = std::bit_width(N - 1);
  if constexpr (numBits <= 8) {
    return std::uint8_t{};
  } else if constexpr (numBits <= 16) {
    return std::uint16_t{};
  } else if constexpr (numBits <= 32) {
    return std::uint32_t{};
  } else {
    return std::uint64_t{};
  }
}

template <std::size_t value>
using smallest_t = decltype(smallest_underlying_type<value>());

// Commented out: struct wrapper approach caused conversion operator ambiguity
// If narrow_cast protection is needed, apply it at specific assignment sites
/*
template <std::size_t value> struct smallest_t {
  using underlying_type = decltype(smallest_underlying_type<value>());
  underlying_type _value{0};
  static constexpr underlying_type min{
      std::numeric_limits<underlying_type>::min()};
  static constexpr underlying_type max{
      std::numeric_limits<underlying_type>::max()};
  static constexpr underlying_type null_sentinel{max};

  constexpr smallest_t() noexcept = default;
  constexpr smallest_t(std::integral auto v) noexcept
      : _value(ptr::narrow_cast<underlying_type>(v)) {}

  constexpr smallest_t(const smallest_t &) noexcept = default;
  constexpr smallest_t(smallest_t &&) noexcept = default;
  constexpr smallest_t &operator=(const smallest_t &) noexcept = default;
  constexpr smallest_t &operator=(smallest_t &&) noexcept = default;
  constexpr operator underlying_type &() noexcept { return _value; }
  constexpr operator underlying_type() const noexcept { return _value; }
};
*/
