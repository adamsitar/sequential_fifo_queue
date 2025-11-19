#pragma once
#include <cstddef>
#include <new>

// Shared offset arithmetic operations for pointer types
template <typename T, typename block_t> struct offset_arithmetic {
  static T *resolve(std::byte *base, size_t offset) {
    return std::launder(reinterpret_cast<T *>(base + offset * sizeof(block_t)));
  }

  static size_t compute_offset(std::byte *base, T *ptr) {
    auto byte_offset = reinterpret_cast<std::byte *>(ptr) - base;
    return static_cast<size_t>(byte_offset) / sizeof(block_t);
  }

  static std::ptrdiff_t byte_offset(std::byte *base, T *ptr) {
    return reinterpret_cast<std::byte *>(ptr) - base;
  }
};
