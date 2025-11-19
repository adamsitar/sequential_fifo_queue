#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <result/result.h>

// Abstract interface for segmented allocators
struct allocator_interface {
  virtual ~allocator_interface() = default;

  virtual result<void *> get_manager(size_t manager_id) = 0;
  virtual result<size_t> find_manager_for_pointer(std::byte *ptr) = 0;

  virtual result<std::byte *> get_segment_base(size_t manager_id,
                                               size_t segment_id) = 0;
  virtual result<size_t> find_segment_in_manager(size_t manager_id,
                                                 std::byte *ptr) = 0;

  virtual result<size_t> compute_offset_in_segment(size_t manager_id,
                                                   size_t segment_id,
                                                   std::byte *ptr,
                                                   size_t elem_size) = 0;
};
