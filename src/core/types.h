#pragma once
#include "result.h"
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory_resource>

#define exforward(x) std::forward<decltype(x)>(x)

template <std::size_t N> constexpr auto smallest_type() {
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

template <std::size_t N> using smallest_t = decltype(smallest_type<N>());

template <size_t... sizes>
concept is_power_of_two = (... && ((sizes & (sizes - 1)) == 0));

template <size_t... values>
concept is_non_zero = (... && (values != 0));

template <size_t... values>
concept nonzero_power_of_two =
    is_power_of_two<values...> && is_non_zero<values...>;

// ============================================================================
// Layered Allocator Concepts
// ============================================================================

/**
 * @brief PMR-compliant allocator
 *
 * Accepts any type derived from std::pmr::memory_resource, including both
 * standard PMR allocators and custom managed allocators.
 */
template <typename T>
concept memory_resource_like = std::derived_from<T, std::pmr::memory_resource>;

/**
 * @brief Provides offset-based addressing (for thin_ptr support)
 *
 * Allocators satisfying this concept enable the thin_ptr optimization by
 * exposing:
 * - unique_tag: Type tag for differentiating thin_ptr instances
 * - offset_type: Small integer type for storing offsets (uint8_t, uint16_t,
 *   etc.)
 * - base(): Returns base pointer of the contiguous memory region
 *
 * thin_ptr performs all offset-to-pointer arithmetic internally using these
 * primitives, providing a transparent pointer-like interface to users.
 *
 * This reduces memory overhead from 8-byte pointers to 1-2 byte offsets.
 */
template <typename T>
concept provides_offset = requires(const T &alloc) {
  typename T::unique_tag;
  typename T::offset_type;
  { alloc.base() } -> std::convertible_to<std::byte *>;
};

/**
 * @brief Provides pointer types for address space management
 *
 * Allocators satisfying this concept expose pointer types for their managed
 * address space. They may provide:
 * 1. Non-template pointer_type: Base pointer for allocator-to-allocator
 * composition
 * 2. Template pointer_type<T>: Typed pointers for data structure type safety
 * 3. Both: Dual interface pattern (recommended for allocators)
 *
 * The dual interface enables clean layering:
 * - Allocators using allocators: use the non-template pointer (no artificial T)
 * - Data structures: use template pointer_type<MyType> (type safety)
 *
 * Example:
 * @code
 * // Allocator exposing dual interface:
 * class my_allocator {
 *   using pointer = thin_ptr<block_type, my_allocator>;  // For composition
 *   template<typename T>
 *   using pointer_type = thin_ptr<T, my_allocator>;      // For data structures
 * };
 * @endcode
 */
template <typename T>
concept provides_pointer = requires { typename T::pointer_type; } || requires {
  typename T::template pointer_type<void>;
};

/**
 * @brief Provides uniform-sized block allocation
 *
 * Allocators satisfying this concept dispense blocks of uniform size and
 * alignment, enabling a simplified parameter-free allocation interface.
 *
 * Benefits:
 * - Cleaner code: allocate_block() vs allocate(2048, 64)
 * - Type safety: Compile-time validation of block characteristics
 * - Zero overhead: No runtime parameters, perfect inlining
 *
 * Note: Uses block_type for type safety. For void* compatibility, use PMR API.
 */
template <typename T>
concept provides_uniform_blocks = requires(T &alloc) {
  typename T::block_type;
  { T::block_size_v } -> std::convertible_to<size_t>;
  { T::block_align_v } -> std::convertible_to<size_t>;
  { T::block_count_v } -> std::convertible_to<size_t>;
  { T::total_size_v } -> std::convertible_to<size_t>;
  { alloc.allocate_block() } -> std::same_as<result<typename T::pointer_type>>;
  {
    alloc.deallocate_block(std::declval<typename T::pointer_type>())
  } -> std::same_as<result<>>;
};

/**
 * @brief Provides management operations
 *
 * Allocators satisfying this concept expose management methods for
 * controlling memory pools or caches.
 */
template <typename T>
concept provides_management = requires(T &alloc, const T &c_alloc) {
  { alloc.reset() } -> std::same_as<void>;
  { c_alloc.size() } -> std::same_as<std::size_t>;
};

template <typename T>
concept homogenous = memory_resource_like<T> && provides_pointer<T> &&
                     provides_uniform_blocks<T> && provides_management<T>;

template <typename T>
concept managed = memory_resource_like<T> && provides_management<T>;

template <typename T>
concept is_nothrow = std::is_nothrow_move_constructible_v<T> &&
                     std::is_nothrow_copy_constructible_v<T> &&
                     std::is_nothrow_destructible_v<T>;
