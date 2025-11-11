#pragma once
#include <cstddef>
#include <cstdint>
#include <result/result.h>

// Type-erased static storage for growing_pool pointer resolution.
template <typename unique_tag> struct segmented_ptr_storage {
  inline static void *_pool_instance{nullptr};

  inline static result<void *> (*_get_manager_fn)(void *, size_t){nullptr};
  inline static result<size_t> (*_find_manager_fn)(void *,
                                                   std::byte *){nullptr};
  inline static result<std::byte *> (*_resolve_ptr_fn)(void *, size_t, size_t,
                                                       size_t){nullptr};
  inline static result<size_t> (*_find_segment_fn)(void *, size_t,
                                                   std::byte *){nullptr};
  inline static result<size_t> (*_compute_offset_fn)(void *, size_t, size_t,
                                                     std::byte *, size_t,
                                                     bool){nullptr};

  // Cache last manager that successfully allocated
  inline static uint8_t _last_alloc_manager{0};
  // Cache last manager that successfully resolved a pointer
  inline static uint8_t _last_lookup_manager{0};

  // Register a growing_pool instance with the storage
  // Only one pool can be registered at a time (enforced by unique_tag)
  template <typename pool_type>
  static result<> register_pool(pool_type *pool) noexcept {
    fail(pool == nullptr, "pool cannot be null");
    fail(_pool_instance != nullptr, "pool already registered");

    _pool_instance = pool;

    // Type-erased callback: get segment_manager by id
    _get_manager_fn = +[](void *pool_ptr, size_t manager_id) -> result<void *> {
      auto *typed_pool = static_cast<pool_type *>(pool_ptr);
      auto manager = ok(typed_pool->get_manager_by_id(manager_id));
      fail(manager == nullptr, "invalid manager id");
      return static_cast<void *>(manager);
    };

    // Type-erased callback: find which manager owns a pointer
    _find_manager_fn = +[](void *pool_ptr, std::byte *ptr) -> result<size_t> {
      auto *typed_pool = static_cast<pool_type *>(pool_ptr);
      return ok(typed_pool->find_manager_for_pointer(ptr));
    };

    // Type-erased callback: resolve pointer from components
    _resolve_ptr_fn = +[](void *pool_ptr, size_t manager_id, size_t segment_id,
                          size_t offset) -> result<std::byte *> {
      auto *typed_pool = static_cast<pool_type *>(pool_ptr);
      auto manager = ok(typed_pool->get_manager_by_id(manager_id));
      fail(manager == nullptr, "invalid manager id");

      std::byte *segment_base = ok(manager->get_segment_base(segment_id));
      return segment_base + offset;
    };

    // Type-erased callback: find which segment in a manager owns a pointer
    _find_segment_fn = +[](void *pool_ptr, size_t manager_id,
                           std::byte *ptr) -> result<size_t> {
      auto *typed_pool = static_cast<pool_type *>(pool_ptr);
      auto manager = ok(typed_pool->get_manager_by_id(manager_id));
      fail(manager == nullptr, "invalid manager id");

      return ok(manager->find_segment_for_pointer(ptr));
    };

    // Type-erased callback: compute offset within a segment
    _compute_offset_fn =
        +[](void *pool_ptr, size_t manager_id, size_t segment_id,
            std::byte *ptr, size_t elem_size, bool is_void) -> result<size_t> {
      auto *typed_pool = static_cast<pool_type *>(pool_ptr);
      auto manager = ok(typed_pool->get_manager_by_id(manager_id));
      fail(manager == nullptr, "invalid manager id");

      std::byte *segment_base = ok(manager->get_segment_base(segment_id));

      auto byte_offset = ptr - segment_base;
      fail(byte_offset < 0, "pointer before segment base");

      if (is_void) {
        // For void*, return byte offset
        return static_cast<size_t>(byte_offset);
      } else {
        // For typed pointers, return block index
        fail(byte_offset % elem_size != 0, "misaligned pointer");
        return static_cast<size_t>(byte_offset / elem_size);
      }
    };

    return {};
  }

  // Unregister the pool (called by pool destructor)
  static void unregister_pool() noexcept {
    _pool_instance = nullptr;
    _get_manager_fn = nullptr;
    _find_manager_fn = nullptr;
    _resolve_ptr_fn = nullptr;
    _find_segment_fn = nullptr;
    _compute_offset_fn = nullptr;
    _last_alloc_manager = 0;
    _last_lookup_manager = 0;
  }

  // Get a segment_manager by its ID (delegates to registered pool)
  template <typename manager_type>
  static result<manager_type *> get_manager(size_t manager_id) noexcept {
    fail(_get_manager_fn == nullptr, "pool not registered");
    fail(_pool_instance == nullptr, "pool instance is null");

    void *manager_ptr = ok(_get_manager_fn(_pool_instance, manager_id));
    return static_cast<manager_type *>(manager_ptr);
  }

  // Find which manager owns a pointer (delegates to registered pool)
  // Uses two-level caching for performance
  static result<size_t> find_manager_for_pointer(std::byte *ptr) noexcept {
    fail(_find_manager_fn == nullptr, "pool not registered");
    fail(_pool_instance == nullptr, "pool instance is null");

    return ok(_find_manager_fn(_pool_instance, ptr));
  }

  // Update allocation cache (called by growing_pool after successful
  // allocation)
  static void update_alloc_cache(uint8_t manager_id) noexcept {
    _last_alloc_manager = manager_id;
  }

  // Update lookup cache (called by growing_pool after successful pointer
  // lookup)
  static void update_lookup_cache(uint8_t manager_id) noexcept {
    _last_lookup_manager = manager_id;
  }

  // Get cached manager IDs (for optimization in growing_pool)
  static uint8_t get_alloc_cache() noexcept { return _last_alloc_manager; }
  static uint8_t get_lookup_cache() noexcept { return _last_lookup_manager; }

  // Resolve a pointer from its components (for growing_pool_ptr)
  template <typename T>
  static result<T *> resolve_pointer(size_t manager_id, size_t segment_id,
                                     size_t offset, size_t elem_size) noexcept {
    fail(_resolve_ptr_fn == nullptr, "pool not registered");
    fail(_pool_instance == nullptr, "pool instance is null");

    // Get base address (offset is in bytes for void*, element count for typed)
    std::byte *base =
        ok(_resolve_ptr_fn(_pool_instance, manager_id, segment_id, 0));

    if constexpr (std::is_void_v<T>) {
      return reinterpret_cast<T *>(base + offset);
    } else {
      return std::launder(reinterpret_cast<T *>(base + (offset * elem_size)));
    }
  }

  // Find which segment in a manager owns a pointer
  static result<size_t> find_segment_in_manager(size_t manager_id,
                                                std::byte *ptr) noexcept {
    fail(_find_segment_fn == nullptr, "pool not registered");
    fail(_pool_instance == nullptr, "pool instance is null");

    return ok(_find_segment_fn(_pool_instance, manager_id, ptr));
  }

  // Compute offset of a pointer within a segment
  static result<size_t> compute_offset_in_segment(size_t manager_id,
                                                  size_t segment_id,
                                                  std::byte *ptr,
                                                  size_t elem_size,
                                                  bool is_void) noexcept {
    fail(_compute_offset_fn == nullptr, "pool not registered");
    fail(_pool_instance == nullptr, "pool instance is null");

    return ok(_compute_offset_fn(_pool_instance, manager_id, segment_id, ptr,
                                 elem_size, is_void));
  }
};
