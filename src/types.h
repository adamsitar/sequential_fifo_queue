#pragma once
#include <cstddef>
#include <expected>
#include <print>
#include <string>

using byte = std::byte;

template<std::size_t N>
constexpr auto
smallestType()
{
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
template<std::size_t N>
using SmallestType = decltype(smallestType<N>());

#define delete_special_member_functions(class_name)                            \
  class_name(const class_name&) = delete;                                      \
  class_name(const class_name&&) = delete;                                     \
  class_name& operator=(const class_name&) = delete;                           \
  class_name& operator=(const class_name&&) = delete;

struct Error
{
  std::string description{ "Undefined" };
};

template<typename U>
using Result = std::expected<U, Error>;

template<size_t size>
concept is_power_of_two = ((size & (size - 1)) == 0);

template<size_t size>
  requires is_power_of_two<size> && (size > 0)
union Block
{
  Block* next_free{ nullptr };
  std::array<std::byte, size> data;
};

#define TRY(expr)                                                              \
  ({                                                                           \
    auto&& result = (expr);                                                    \
    if (!result) {                                                             \
      return std::unexpected(result.error());                                  \
    }                                                                          \
    *result;                                                                   \
  })

#define expose_iterators(BASE_TYPE)                                            \
  using BASE_TYPE::begin;                                                      \
  using BASE_TYPE::end;                                                        \
  using BASE_TYPE::cbegin;                                                     \
  using BASE_TYPE::cend;                                                       \
  using BASE_TYPE::rbegin;                                                     \
  using BASE_TYPE::rend;                                                       \
  using BASE_TYPE::crbegin;                                                    \
  using BASE_TYPE::crend;
