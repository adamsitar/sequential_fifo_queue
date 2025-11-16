#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <result/result.h>

// Abstract interface for segmented allocators (replaces function pointer table)
// Enables type-erased access to growing_pool operations with compiler-optimized
// virtual dispatch
struct allocator_interface {
  virtual ~allocator_interface() = default;

  // Manager-level operations
  virtual result<void *> get_manager(size_t manager_id) = 0;
  virtual result<size_t> find_manager_for_pointer(std::byte *ptr) = 0;

  // Segment-level operations (within a specific manager context)
  virtual result<std::byte *> get_segment_base(size_t manager_id,
                                               size_t segment_id) = 0;
  virtual result<size_t> find_segment_in_manager(size_t manager_id,
                                                 std::byte *ptr) = 0;

  // Offset computation (within a specific segment context)
  virtual result<size_t> compute_offset_in_segment(size_t manager_id,
                                                   size_t segment_id,
                                                   std::byte *ptr,
                                                   size_t elem_size) = 0;
};
