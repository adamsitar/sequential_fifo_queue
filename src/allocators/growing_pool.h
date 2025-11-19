#pragma once
#include "ptr_utils.h"
#include <bit>
#include <cassert>
#include <cstddef>
#include <intrusive_slist.h>
#include <local_buffer.h>
#include <memory_resource>
#include <pointers/allocator_interface.h>
#include <pointers/cache.h>
#include <pointers/growing_pool_storage.h>
#include <pointers/segmented_ptr.h>
#include <result/result.h>
#include <segment_manager.h>
#include <types.h>

// Growing pool allocator - unlimited capacity via linked list of
// segment_managers.
template <size_t block_size_v, size_t max_manager_count_v,
          is_homogenous upstream_t, typename tag = void>
  requires is_power_of_two<block_size_v>
class unique_growing_pool : public std::pmr::memory_resource,
                            public allocator_interface {
public:
  using manager_type = segment_manager<block_size_v, upstream_t>;
  using block_type = typename manager_type::block_type;

  static constexpr size_t max_managers = max_manager_count_v;
  // static constexpr size_t max_managers = upstream_t::max_block_count;

  static constexpr size_t max_block_count =
      manager_type::max_block_count * max_managers;
  static constexpr size_t total_size = block_size_v * max_block_count;
  static constexpr size_t block_size = block_size_v;
  static constexpr size_t block_align = block_size_v;
  using unique_tag = tag;

  using pointer_type =
      basic_segmented_ptr<block_type, block_type,
                          manager_type::blocks_per_segment,
                          manager_type::max_segments, max_managers, tag>;
  static constexpr size_t pointer_size{sizeof(pointer_type)};
  // static_assert(pointer_size <= 1, "pointer size is not smaller than 1");

  static constexpr size_t offset_bits = pointer_type::offset_bits;
  static constexpr size_t segment_bits = pointer_type::segment_bits;
  static constexpr size_t manager_bits = pointer_type::manager_bits;

  static_assert(offset_bits > 0, "offset_bits must be at least 1");
  static_assert(segment_bits > 0, "segment_bits must be at least 1");
  static_assert(manager_bits > 0, "manager_bits must be at least 1");

private:
  struct manager_node;
  using manager_node_ptr =
      typename upstream_t::pointer_type::template rebind<manager_node>;

  struct manager_node {
    manager_type manager{};
    manager_node_ptr next{nullptr};
  };

  static_assert(sizeof(manager_node) <= upstream_t::block_size,
                "manager_node must exactly match upstream block_size for "
                "pointer arithmetic");

  upstream_t *_upstream;
  intrusive_slist<manager_node_ptr> _managers;
  smallest_t<max_managers> _manager_count{0};

  using storage = segmented_ptr_storage<tag>;
  using alloc_cache = alloc_hint_cache<tag>;
  using lookup_cache = lookup_hint_cache<tag>;

public:
  explicit unique_growing_pool(upstream_t *upstream) : _upstream(upstream) {
    fatal(upstream == nullptr, "upstream allocator cannot be null");
    unwrap(storage::register_pool(this));
  }

  ~unique_growing_pool() override {
    storage::unregister_pool();
    alloc_cache::reset();
    lookup_cache::reset();

    while (!_managers.empty()) {
      manager_node_ptr curr = _managers.pop_front();
      curr->manager.cleanup(_upstream);
      curr->manager.~manager_type();
      typename upstream_t::pointer_type upstream_ptr(static_cast<void *>(curr));
      unwrap(_upstream->deallocate_block(upstream_ptr));
    }
  }

  unique_growing_pool(const unique_growing_pool &) = delete;
  unique_growing_pool &operator=(const unique_growing_pool &) = delete;
  unique_growing_pool(unique_growing_pool &&) = delete;
  unique_growing_pool &operator=(unique_growing_pool &&) = delete;

  result<pointer_type> allocate_block() noexcept {
    uint8_t cached_mgr = alloc_cache::get();
    if (cached_mgr < _manager_count) {
      auto manager = ok(get_manager_by_id(cached_mgr));
      auto block_result = manager->try_allocate(_upstream);
      if (block_result) {
        return encode_pointer(cached_mgr, manager, *block_result);
      }
    }

    // Scan existing managers
    size_t id = _manager_count;
    for (auto &manager_node : _managers) {
      --id;
      if (id != cached_mgr) {
        auto block_result = manager_node.manager.try_allocate(_upstream);
        if (block_result) {
          alloc_cache::set(id);
          return encode_pointer(id, &manager_node.manager, *block_result);
        }
      }
    }

    return allocate_new_manager();
  }

  result<> deallocate_block(pointer_type ptr) noexcept {
    fail(ptr == nullptr, "cannot deallocate null pointer");

    size_t manager_id = ptr.get_manager_id();
    fail(manager_id >= _manager_count, "invalid manager ID");

    auto manager = ok(get_manager_by_id(manager_id));

    auto *block = static_cast<block_type *>(static_cast<void *>(ptr));
    ok(manager->deallocate(block, _upstream));
    // TODO: deallocate empty managers to reclaim memory

    return {};
  }

  void reset() {
    for (auto manager : _managers) {
      manager.node()->manager.reset(_upstream);
    }
    alloc_cache::reset();
    lookup_cache::reset();
  }

  std::size_t size() const noexcept {
    std::size_t total = 0;
    for (auto manager : _managers) {
      total += manager.node()->manager.available_count();
    }
    return total;
  }

  result<manager_type *> get_manager_by_id(size_t id) noexcept {
    fatal(id >= _manager_count, "ID greater than total count of managers");
    size_t current_id = _manager_count - 1;

    for (auto &manager_node : _managers) {
      if (current_id == id) { return &manager_node.manager; }
      if (current_id == 0) { break; }
      --current_id;
    }

    return "manager for this ID not found";
  }

  result<size_t> find_manager_for_pointer(std::byte *ptr) const noexcept {
    auto *block = reinterpret_cast<block_type *>(ptr);

    uint8_t cached_alloc = alloc_cache::get();
    uint8_t cached_lookup = lookup_cache::get();

    if (cached_alloc < _manager_count) {
      auto manager =
          ok(const_cast<unique_growing_pool *>(this)->get_manager_by_id(
              cached_alloc));
      if (manager && manager->owns(block)) {
        lookup_cache::set(cached_alloc);
        return cached_alloc;
      }
    }

    if (cached_lookup < _manager_count && cached_lookup != cached_alloc) {
      auto manager =
          ok(const_cast<unique_growing_pool *>(this)->get_manager_by_id(
              cached_lookup));
      if (manager && manager->owns(block)) { return cached_lookup; }
    }

    size_t id = _manager_count;
    for (auto &manager_node : _managers) {
      --id;
      if (id != cached_alloc && id != cached_lookup) {
        if (manager_node.manager.owns(block)) {
          lookup_cache::set(id);
          return id;
        }
      }
    }

    return "pointer not owned";
  }

  // allocator_interface implementation

  result<void *> get_manager(size_t manager_id) override {
    auto *mgr = ok(get_manager_by_id(manager_id));
    fail(mgr == nullptr, "invalid manager id");
    return static_cast<void *>(mgr);
  }

  result<size_t> find_manager_for_pointer(std::byte *ptr) override {
    // Explicitly call the const version to avoid infinite recursion
    return const_cast<const unique_growing_pool *>(this)
        ->find_manager_for_pointer(ptr);
  }

  result<std::byte *> get_segment_base(size_t manager_id,
                                       size_t segment_id) override {
    auto *mgr = ok(get_manager_by_id(manager_id));
    fail(mgr == nullptr, "invalid manager id");
    return ok(mgr->get_segment_base(segment_id));
  }

  result<size_t> find_segment_in_manager(size_t manager_id,
                                         std::byte *ptr) override {
    auto *mgr = ok(get_manager_by_id(manager_id));
    fail(mgr == nullptr, "invalid manager id");
    return ok(mgr->find_segment_for_pointer(ptr));
  }

  result<size_t> compute_offset_in_segment(size_t manager_id, size_t segment_id,
                                           std::byte *ptr,
                                           size_t elem_size) override {
    auto *mgr = ok(get_manager_by_id(manager_id));
    fail(mgr == nullptr, "invalid manager id");

    std::byte *segment_base = ok(mgr->get_segment_base(segment_id));

    auto byte_offset = ptr - segment_base;
    fail(byte_offset < 0, "pointer before segment base");

    // Return block index
    fail(byte_offset % elem_size != 0, "misaligned pointer");
    return static_cast<size_t>(byte_offset / elem_size);
  }

private:
  result<pointer_type> encode_pointer(size_t manager_id, manager_type *manager,
                                      block_type *block) noexcept {
    auto segment_id = ok(manager->find_segment_for_pointer(
        reinterpret_cast<std::byte *>(block)));
    std::byte *segment_base = ok(manager->get_segment_base(segment_id));
    auto byte_offset = reinterpret_cast<std::byte *>(block) - segment_base;
    fatal(byte_offset < 0, "block before segment base");
    size_t offset = byte_offset / block_size_v;

    return pointer_type(manager_id, segment_id, offset);
  }

  result<pointer_type> allocate_new_manager() noexcept {
    fail(_manager_count >= max_managers, "manager limit reached");

    auto upstream_block = ok(_upstream->allocate_block());
    void *node_ptr = static_cast<void *>(upstream_block);
    auto *new_node = new (node_ptr) manager_node();

    manager_node_ptr node_ptr_typed(new_node);
    _managers.push_front(node_ptr_typed);

    size_t new_id = _manager_count++;
    alloc_cache::set(new_id);

    return allocate_block();
  }

  void *do_allocate(size_t bytes, size_t alignment) override {
    if (bytes > block_size_v || alignment > block_size_v) { return nullptr; }

    auto result = allocate_block();
    return result ? static_cast<void *>(*result) : nullptr;
  }

  void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
    if (ptr == nullptr) { return; }

    pointer_type growing_ptr{ptr};
    unwrap(deallocate_block(growing_ptr));
  }

  bool
  do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }
};

#define growing_pool(block_size, manager_count, upstream_type)                 \
  unique_growing_pool<block_size, manager_count, upstream_type, decltype([] {})>

static_assert(is_homogenous<growing_pool(8, 32, local_buffer(16, 128))>,
              "growing_pool must implement homogenous concept");
