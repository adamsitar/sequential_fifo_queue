#pragma once
#include <offset_list.h>
#include <result/result.h>
#include <ring_buffer.h>
#include <types.h>

template <typename local_buffer_type, typename dynamic_buffer_type>
struct queue_allocator_storage {
  inline static local_buffer_type *_local_alloc{nullptr};
  inline static dynamic_buffer_type *_list_alloc{nullptr};
};

// FIFO queue implemented as a linked list of ring buffers.
template <is_nothrow T, size_t ring_buffer_capacity,
          is_homogenous local_buffer_type, is_homogenous dynamic_buffer_type>
class queue {
public:
  using ring_buffer_type =
      ring_buffer<T, ring_buffer_capacity, local_buffer_type>;
  using storage =
      queue_allocator_storage<local_buffer_type, dynamic_buffer_type>;

private:
  struct ring_buffer_node {
    ring_buffer_type buffer;
    explicit ring_buffer_node(local_buffer_type *alloc) : buffer(alloc) {}
  };

  offset_list<ring_buffer_node, dynamic_buffer_type> _list;

public:
  static_assert(sizeof(ring_buffer_node) <= dynamic_buffer_type::block_size,
                "DynamicBuffer block_size too small for ring_buffer_node");
  static_assert(dynamic_buffer_type::block_size % alignof(ring_buffer_node) ==
                    0,
                "DynamicBuffer block_size must be multiple of ring_buffer_node "
                "alignment");
  static_assert(ring_buffer_type::storage_bytes_v <=
                    local_buffer_type::block_size,
                "LocalBuffer block_size too small for ring_buffer storage");

  explicit queue(local_buffer_type *local_alloc,
                 dynamic_buffer_type *list_alloc)
      : _list(list_alloc) {
    fatal(local_alloc == nullptr, "Local allocator cannot be null");
    fatal(list_alloc == nullptr, "List allocator cannot be null");

    storage::_local_alloc = local_alloc;
    storage::_list_alloc = list_alloc;
  }

  ~queue() {
    clear();
    // NOTE: Don't clear static storage here when using multiple instances.
    // All instances of the same queue type share static storage, so clearing
    // it would break other instances. The storage is overwritten on
    // construction.
    // storage::_local_alloc = nullptr;
    // storage::_list_alloc = nullptr;
  }

  queue(const queue &) = delete;
  queue &operator=(const queue &) = delete;
  queue(queue &&) = delete;
  queue &operator=(queue &&) = delete;

  template <typename U>
    requires std::constructible_from<T, U>
  result<> push(U &&value) noexcept {
    if (_list.is_empty() ||
        const_cast<ring_buffer_node *>(ok(_list.front()))->buffer.is_full()) {
      ok(allocate_new_ring_buffer());
    }

    const_cast<ring_buffer_node *>(ok(_list.front()))
        ->buffer.push(std::forward<U>(value));
    return {};
  }

  template <typename... Args>
    requires std::constructible_from<T, Args...>
  result<> emplace(Args &&...args) noexcept {
    if (_list.is_empty() ||
        const_cast<ring_buffer_node *>(ok(_list.front()))->buffer.is_full()) {
      ok(allocate_new_ring_buffer());
    }

    const_cast<ring_buffer_node *>(ok(_list.front()))
        ->buffer.emplace(std::forward<Args>(args)...);
    return {};
  }

  result<T> pop() noexcept {
    fail(empty(), "Cannot pop from empty queue");

    auto *pop_node = const_cast<ring_buffer_node *>(ok(_list.back()));
    T value = ok(pop_node->buffer.pop());

    if (pop_node->buffer.empty()) { deallocate_back_ring_buffer(); }

    return value;
  }

  void clear() noexcept { _list.clear(); }

  result<const T *> front() const noexcept {
    fail(empty(), "front() called on empty queue");
    return &ok(_list.back())->buffer.front();
  }

  result<const T *> back() const noexcept {
    fail(empty(), "back() called on empty queue");
    return &ok(_list.front())->buffer.back();
  }

  bool empty() const noexcept { return _list.is_empty(); }

  // O(n) where n is number of ring_buffers
  size_t size() const noexcept {
    size_t total = 0;
    int counter = 0;
    // for (const auto &node : _list) {
    //   total += node.buffer.size();
    // }
    auto begin = _list.begin();
    auto end = _list.end();

    for (auto it = begin; it != end; ++it) {
      const auto &node = *it; // dereference iterator
      total += node.buffer.size();
    }
    return total;
  }

private:
  result<> allocate_new_ring_buffer() noexcept {
    ok(_list.emplace_front(storage::_local_alloc));
    return {};
  }

  result<> deallocate_back_ring_buffer() noexcept {
    fail(_list.is_empty(), "Cannot deallocate from empty list");

    ok(_list.erase_back());
    return {};
  }
};
