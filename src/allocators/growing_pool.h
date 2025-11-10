#pragma once
#include <bit>
#include <cassert>
#include <cstddef>
#include <local_buffer.h>
#include <memory_resource>
#include <pointers/growing_pool_ptr.h>
#include <pointers/growing_pool_storage.h>
#include <result/result.h>
#include <segment_manager.h>
#include <types.h>

// Growing pool allocator - unlimited capacity via linked list of
// segment_managers. Each segment_manager has a fixed capacity (~4 segments),
// but the pool can allocate new managers on demand, until upstream exhaustion.
template <size_t block_size_v, size_t max_manager_count_v,
          homogenous upstream_t, typename tag = void>
  requires is_power_of_two<block_size_v>
class unique_growing_pool : public std::pmr::memory_resource {
public:
  using manager_type = segment_manager<block_size_v, upstream_t>;
  using block_type = typename manager_type::block_type;

  static constexpr size_t max_managers = max_manager_count_v;
  // static constexpr size_t max_managers = upstream_t::max_block_count;

  // provides_uniform_blocks
  static constexpr size_t max_block_count =
      manager_type::max_block_count * max_managers;
  static constexpr size_t total_size = block_size_v * max_block_count;
  static constexpr size_t block_size = block_size_v;
  static constexpr size_t block_align = block_size_v;
  using unique_tag = tag;

  using pointer_type =
      basic_growing_pool_ptr<block_type, manager_type::blocks_per_segment,
                             manager_type::max_segments, max_managers, tag>;

  // Bit widths are now calculated in the pointer type, expose them here for convenience
  static constexpr size_t offset_bits = pointer_type::offset_bits;
  static constexpr size_t segment_bits = pointer_type::segment_bits;
  static constexpr size_t manager_bits = pointer_type::manager_bits;

  static_assert(offset_bits > 0, "offset_bits must be at least 1");
  static_assert(segment_bits > 0, "segment_bits must be at least 1");
  static_assert(manager_bits > 0, "manager_bits must be at least 1");

private:
  struct manager_node {
    manager_type manager{};
    typename upstream_t::pointer_type next{nullptr};
  };

  static_assert(sizeof(manager_node) <= upstream_t::block_size,
                "manager_node must fit within upstream block");

  upstream_t *_upstream;
  typename upstream_t::pointer_type _head{nullptr};
  smallest_t<max_managers> _manager_count{0};
  smallest_t<max_managers> _alloc_cache{0};
  mutable smallest_t<max_managers> _lookup_cache{0};

  using storage = growing_pool_storage<tag>;

public:
  explicit unique_growing_pool(upstream_t *upstream) : _upstream(upstream) {
    fatal(upstream == nullptr, "upstream allocator cannot be null");
    unwrap(storage::register_pool(this));
  }

  ~unique_growing_pool() override {
    storage::unregister_pool();

    // Destroy all manager nodes
    auto curr = _head;
    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      auto next = curr_node->next;
      // Cleanup segments before destroying manager
      curr_node->manager.cleanup(_upstream);
      // Manually destroy manager
      curr_node->manager.~manager_type();
      // Deallocate the node memory back to upstream
      unwrap(_upstream->deallocate_block(curr));
      curr = next;
    }
  }

  unique_growing_pool(const unique_growing_pool &) = delete;
  unique_growing_pool &operator=(const unique_growing_pool &) = delete;
  unique_growing_pool(unique_growing_pool &&) = delete;
  unique_growing_pool &operator=(unique_growing_pool &&) = delete;

  result<pointer_type> allocate_block() noexcept {
    if (_alloc_cache < _manager_count) {
      auto manager = ok(get_manager_by_id(_alloc_cache));
      auto block_result = manager->try_allocate(_upstream);

      // Check if successful without propagating errors
      if (block_result) {
        return encode_pointer(_alloc_cache, manager, *block_result);
      }
      // If failed, fall through to try other managers or create new one
    }

    // Scan existing managers
    auto curr = _head;
    size_t id = 0;
    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      if (id != _alloc_cache) {
        auto block_result = curr_node->manager.try_allocate(_upstream);
        // Check if successful without propagating errors
        if (block_result) {
          _alloc_cache = id;
          storage::update_alloc_cache(id);
          return encode_pointer(id, &curr_node->manager, *block_result);
        }
        // If failed, continue to next manager
      }
      curr = curr_node->next;
      ++id;
    }

    return allocate_new_manager();
  }

  // Deallocate a block
  result<> deallocate_block(pointer_type ptr) noexcept {
    fail(ptr == nullptr, "cannot deallocate null pointer");

    size_t manager_id = ptr.get_manager_id();
    fail(manager_id >= _manager_count, "invalid manager ID");

    auto manager = ok(get_manager_by_id(manager_id));

    auto *block = static_cast<block_type *>(static_cast<void *>(ptr));
    ok(manager->deallocate(block, _upstream));

    // Optional: deallocate empty managers to reclaim memory
    // For now, keep managers alive for simplicity

    return {};
  }

  // Reset all managers to initial state
  void reset() noexcept {
    auto curr = _head;
    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      curr_node->manager.reset(_upstream);
      curr = curr_node->next;
    }
    _alloc_cache = 0;
    _lookup_cache = 0;
  }

  // Get total number of available blocks across all managers
  std::size_t size() const noexcept {
    std::size_t total = 0;
    auto curr = _head;
    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      total += curr_node->manager.available_count();
      curr = curr_node->next;
    }
    return total;
  }

  // Get manager by ID (walks linked list)
  result<manager_type *> get_manager_by_id(size_t id) noexcept {
    fail(id >= _manager_count, "ID greater than total count of managers");

    auto curr = _head;

    // List is prepended, so reverse order
    size_t current_id = _manager_count - 1;

    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      if (current_id == id) { return &curr_node->manager; }
      curr = curr_node->next;
      if (current_id == 0) break;
      --current_id;
    }

    return "manager for this ID not found";
  }

  // Find which manager owns a pointer (with two-level caching)
  result<size_t> find_manager_for_pointer(std::byte *ptr) const noexcept {
    auto *block = reinterpret_cast<const block_type *>(ptr);

    // Try alloc cache (spatial locality)
    if (_alloc_cache < _manager_count) {
      auto manager =
          ok(const_cast<unique_growing_pool *>(this)->get_manager_by_id(
              _alloc_cache));
      if (manager && manager->owns(block)) {
        _lookup_cache = _alloc_cache;
        return _alloc_cache;
      }
    }

    // Try lookup cache (temporal locality)
    if (_lookup_cache < _manager_count && _lookup_cache != _alloc_cache) {
      auto manager =
          ok(const_cast<unique_growing_pool *>(this)->get_manager_by_id(
              _lookup_cache));
      if (manager && manager->owns(block)) { return _lookup_cache; }
    }

    // Linear scan (cold path)
    auto curr = _head;
    size_t id = _manager_count;
    while (curr != nullptr) {
      auto *curr_node = static_cast<manager_node *>(static_cast<void *>(curr));
      --id;
      if (id != _alloc_cache && id != _lookup_cache) {
        if (curr_node->manager.owns(block)) {
          _lookup_cache = id;
          return id;
        }
      }
      curr = curr_node->next;
    }

    return "pointer not owned";
  }

private:
  // Encode a block pointer into a growing_pool_ptr
  result<pointer_type> encode_pointer(size_t manager_id, manager_type *manager,
                                      block_type *block) noexcept {
    // Find which segment owns this block
    auto segment_id_result =
        manager->find_segment_for_pointer(reinterpret_cast<std::byte *>(block));
    fail(!segment_id_result, "block not owned by manager");

    size_t segment_id = *segment_id_result;

    // Compute offset within segment
    std::byte *segment_base =
        ok(manager->get_segment_base(static_cast<uint8_t>(segment_id)));
    auto *block_ptr = reinterpret_cast<std::byte *>(block);

    auto byte_offset = block_ptr - segment_base;
    fail(byte_offset < 0, "block before segment base");

    size_t offset = static_cast<size_t>(byte_offset) / block_size_v;

    return pointer_type(manager_id, segment_id, offset);
  }

  // Allocate a new segment_manager and retry allocation
  result<pointer_type> allocate_new_manager() noexcept {
    fail(_manager_count >= max_managers, "manager limit reached");

    // Allocate a block from upstream for the manager_node
    auto upstream_block = ok(_upstream->allocate_block());
    void *node_ptr = static_cast<void *>(upstream_block);

    // Placement-new the manager_node (default constructed)
    auto *new_node = new (node_ptr) manager_node();

    // Prepend to linked list (O(1))
    new_node->next = _head;
    _head = upstream_block; // Store the upstream pointer, not raw pointer

    size_t new_id = _manager_count++;
    _alloc_cache = new_id;
    storage::update_alloc_cache(new_id);

    // Recursively allocate from the new manager
    return allocate_block();
  }

  // std::pmr::memory_resource overrides
  void *do_allocate(size_t bytes, size_t alignment) override {
    if (bytes > block_size_v || alignment > block_size_v) { return nullptr; }

    auto result = allocate_block();
    return result ? static_cast<void *>(*result) : nullptr;
  }

  void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
    if (ptr == nullptr) return;

    pointer_type growing_ptr{ptr};
    unwrap(deallocate_block(growing_ptr));
  }

  bool
  do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    return this == &other;
  }
};

// Macro for creating unique growing_pool instances
#define growing_pool(block_size, manager_count, upstream_type)                 \
  unique_growing_pool<block_size, manager_count, upstream_type, decltype([] {})>

static_assert(homogenous<growing_pool(8, 8, local_buffer(16, 128))>,
              "growing_pool must implement homogenous concept");
