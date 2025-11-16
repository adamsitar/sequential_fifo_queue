#pragma once
#include <concepts>
#include <cstddef>
#include <memory_resource>
#include <result/result.h>
#include <type_traits>

// Compile-time value concepts
template <size_t... sizes>
concept is_power_of_two = (... && ((sizes & (sizes - 1)) == 0));

template <size_t... values>
concept is_non_zero = (... && (values != 0));

template <size_t... values>
concept nonzero_power_of_two =
    is_power_of_two<values...> && is_non_zero<values...>;

// Allocator interface concepts
template <typename T>
concept memory_resource_like = std::derived_from<T, std::pmr::memory_resource>;

// Provides offset-based addressing
template <typename T>
concept provides_offset = requires(const T &alloc) {
  typename T::unique_tag;
  typename T::offset_type;
  { alloc.base() } -> std::convertible_to<std::byte *>;
};

// Provides custom pointer type with rebind support
template <typename T>
concept provides_pointer = requires {
  typename T::pointer_type;
  typename T::pointer_type::template rebind<void>;
};

template <typename T>
concept provides_uniform_blocks = requires(T &alloc) {
  typename T::block_type;
  { T::block_size } -> std::convertible_to<size_t>;
  { T::block_align } -> std::convertible_to<size_t>;
  { T::max_block_count } -> std::convertible_to<size_t>;
  { T::total_size } -> std::convertible_to<size_t>;
  { alloc.allocate_block() } -> std::same_as<result<typename T::pointer_type>>;
  {
    alloc.deallocate_block(std::declval<typename T::pointer_type>())
  } -> std::same_as<result<>>;
};

template <typename T>
concept provides_management = requires(T &alloc, const T &c_alloc) {
  { alloc.reset() } -> std::same_as<void>;
  { c_alloc.size() } -> std::same_as<std::size_t>;
};

template <typename T>
concept managed = memory_resource_like<T> && provides_management<T>;

template <typename T>
concept is_homogenous =
    managed<T> && provides_uniform_blocks<T> && provides_pointer<T>;

// Contiguous allocators provide offset-based addressing
template <typename T>
concept contiguous_allocator = is_homogenous<T> && provides_offset<T>;

// Type property concepts
template <typename T>
concept is_nothrow = std::is_nothrow_destructible_v<T>;
