#pragma once
#include <cstddef>
#include <iterators/container_interface.h>
#include <iterators/iterator_facade.h>

// ============================================================================
// Intrusive Singly-Linked List
// ============================================================================
// Header-only intrusive linked list for managing pre-allocated nodes.
//
// Requirements:
//   - NodePtr must be a pointer-like type (raw pointer or custom pointer)
//   - *NodePtr must have a 'next' field of type NodePtr
//   - NodePtr must support: ==, !=, nullptr comparison, operator*
//
// Key properties:
//   - Does NOT allocate - caller provides nodes
//   - Caller must deallocate nodes after removal
//   - Works with custom pointer types (thin_ptr, segmented_ptr, etc.)
//   - Zero overhead - just pointer manipulation
//
// Use cases:
//   - Allocator internal structures (growing_pool)
//   - Kernel-style lists
//   - Memory-constrained environments
// ============================================================================

template <typename node_ptr>
concept intrusive_node = requires(node_ptr ptr) {
  { ptr->next } -> std::convertible_to<node_ptr>;
  { ptr == nullptr } -> std::convertible_to<bool>;
  { ptr != nullptr } -> std::convertible_to<bool>;
  { *ptr };
};

template <intrusive_node node_ptr>
class intrusive_slist
    : public forward_container_iterator_interface<intrusive_slist<node_ptr>> {
public:
  using value_type =
      std::remove_reference_t<decltype(*std::declval<node_ptr>())>;
  using size_type = std::size_t;

private:
  node_ptr _head{nullptr};
  size_type _count{0};

public:
  intrusive_slist() = default;

  // Non-copyable, non-movable (contains external node pointers)
  intrusive_slist(const intrusive_slist &) = delete;
  intrusive_slist &operator=(const intrusive_slist &) = delete;
  intrusive_slist(intrusive_slist &&) = delete;
  intrusive_slist &operator=(intrusive_slist &&) = delete;

  ~intrusive_slist() = default;

  // ========================================================================
  // Modifiers
  // ========================================================================

  void push_front(node_ptr node) noexcept {
    node->next = _head;
    _head = node;
    ++_count;
  }

  node_ptr pop_front() noexcept {
    auto old_head = _head;
    _head = _head->next;
    --_count;
    return old_head; // Caller must deallocate
  }

  void clear() noexcept {
    _head = nullptr;
    _count = 0;
  }

  // ========================================================================
  // Accessors
  // ========================================================================

  node_ptr front() const noexcept { return _head; }
  bool empty() const noexcept { return _head == nullptr; }
  size_type size() const noexcept { return _count; }

  // O(n) indexed access - walks the list
  node_ptr get(size_type index) const noexcept {
    if (index >= _count) { return nullptr; }

    auto current = _head;
    for (size_type i = 0; i < index && current != nullptr; ++i) {
      current = current->next;
    }
    return current;
  }

  class iterator : public forward_iterator_facade<value_type> {
    node_ptr _current;

  public:
    iterator() : _current(nullptr) {}
    explicit iterator(node_ptr p) : _current(p) {}

    value_type &dereference() const noexcept { return *_current; }
    void increment() noexcept { _current = _current->next; }
    bool equals(const iterator &other) const noexcept {
      return _current == other._current;
    }
    node_ptr node() const noexcept { return _current; }
  };

  iterator begin() const noexcept { return iterator{_head}; }
  iterator end() const noexcept { return iterator{nullptr}; }

  void insert_after(iterator pos, node_ptr node) noexcept {
    if (pos.node() == nullptr) {
      push_front(node);
    } else {
      node->next = pos.node()->next;
      pos.node()->next = node;
      ++_count;
    }
  }

  node_ptr erase_after(iterator pos) noexcept {
    if (pos.node() == nullptr || pos.node()->next == nullptr) {
      return nullptr;
    }

    auto to_erase = pos.node()->next;
    pos.node()->next = to_erase->next;
    --_count;
    return to_erase; // caller must deallocate
  }

  // linear search
  iterator find(node_ptr node) const noexcept {
    for (auto it = begin(); it != end(); ++it) {
      if (it.node() == node) { return it; }
    }
    return end();
  }

  // O(n)
  bool remove(node_ptr node) noexcept {
    if (_head == node) {
      pop_front();
      return true;
    }

    auto current = _head;
    while (current != nullptr && current->next != nullptr) {
      if (current->next == node) {
        current->next = node->next;
        --_count;
        return true;
      }
      current = current->next;
    }
    return false;
  }
};
