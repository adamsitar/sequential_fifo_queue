#pragma once
#include <cstddef>
#include <new>

// Shared offset arithmetic operations for pointer types
// Extracts common Level 0 resolution logic used by both thin_ptr and
// segmented_ptr
template <typename T, typename block_t> struct offset_arithmetic {
  // Resolve a pointer from base address and block offset
  static T *resolve(std::byte *base, size_t offset) {
    return std::launder(
        reinterpret_cast<T *>(base + offset * sizeof(block_t)));
  }

  // Compute block offset from base address to pointer
  static size_t compute_offset(std::byte *base, T *ptr) {
    auto byte_offset = reinterpret_cast<std::byte *>(ptr) - base;
    return static_cast<size_t>(byte_offset) / sizeof(block_t);
  }

  // Compute byte offset from base address to pointer
  static std::ptrdiff_t byte_offset(std::byte *base, T *ptr) {
    return reinterpret_cast<std::byte *>(ptr) - base;
  }
};
