#pragma once
#include "allocator_interface.h"
#include "offset_arithmetic.h"
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <result/result.h>

// Type-erased static storage for growing_pool pointer resolution.
template <typename unique_tag> struct segmented_ptr_storage {
  inline static allocator_interface *_interface{nullptr};

  template <typename pool_type>
    requires std::derived_from<pool_type, allocator_interface>
  static result<> register_pool(pool_type *pool) noexcept {
    fail(pool == nullptr, "pool cannot be null");

    fail(_interface != nullptr, "pool already registered");

    _interface = static_cast<allocator_interface *>(pool);

    return {};
  }

  static void unregister_pool() noexcept { _interface = nullptr; }

  // Get a segment_manager by its ID (delegates to registered pool)
  template <typename manager_type>
  static result<manager_type *> get_manager(size_t manager_id) noexcept {
    fail(_interface == nullptr, "pool not registered");

    void *manager_ptr = ok(_interface->get_manager(manager_id));
    return static_cast<manager_type *>(manager_ptr);
  }

  // Find which manager owns a pointer (delegates to registered pool)
  static result<size_t> find_manager_for_pointer(std::byte *ptr) noexcept {
    fail(_interface == nullptr, "pool not registered");
    return ok(_interface->find_manager_for_pointer(ptr));
  }

  template <typename T, typename block_t>
  static result<T *> resolve_pointer(size_t manager_id, size_t segment_id,
                                     size_t offset) noexcept {
    fail(_interface == nullptr, "pool not registered");
    std::byte *segment_base =
        ok(_interface->get_segment_base(manager_id, segment_id));

    // Use shared offset arithmetic (Level 0 resolution)
    return offset_arithmetic<T, block_t>::resolve(segment_base, offset);
  }

  // Find which segment in a manager owns a pointer
  static result<size_t> find_segment_in_manager(size_t manager_id,
                                                std::byte *ptr) noexcept {
    fail(_interface == nullptr, "pool not registered");
    return ok(_interface->find_segment_in_manager(manager_id, ptr));
  }

  // Compute offset of a pointer within a segment
  static result<size_t> compute_offset_in_segment(size_t manager_id,
                                                  size_t segment_id,
                                                  std::byte *ptr,
                                                  size_t elem_size) noexcept {
    fail(_interface == nullptr, "pool not registered");
    return ok(_interface->compute_offset_in_segment(manager_id, segment_id, ptr,
                                                    elem_size));
  }
};
