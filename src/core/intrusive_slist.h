#pragma once
#include <cstddef>
#include <iterators/container_interface.h>
#include <iterators/iterator_facade.h>
#include <print>

// intrusive linked list for managing pre-allocated nodes.
template <typename node_ptr>
concept intrusive_node = requires(node_ptr ptr) {
  { ptr->next } -> std::convertible_to<node_ptr>;
  { ptr == nullptr } -> std::convertible_to<bool>;
  { ptr != nullptr } -> std::convertible_to<bool>;
  { *ptr };
};

template <intrusive_node node_ptr>
class intrusive_slist
    : public forward_iterator_interface<intrusive_slist<node_ptr>> {
public:
  using value_type =
      std::remove_reference_t<decltype(*std::declval<node_ptr>())>;
  using size_type = std::size_t;

private:
  node_ptr _head{nullptr};
  node_ptr _tail{nullptr};
  size_type _count{0};

public:
  intrusive_slist() = default;
  // Non-copyable, non-movable (contains external node pointers)
  intrusive_slist(const intrusive_slist &) = delete;
  intrusive_slist &operator=(const intrusive_slist &) = delete;
  intrusive_slist(intrusive_slist &&) = delete;
  intrusive_slist &operator=(intrusive_slist &&) = delete;
  ~intrusive_slist() = default;

  void push_front(node_ptr node) noexcept {
    node->next = _head;
    _head = node;

    if (_tail == nullptr) { _tail = node; }
    ++_count;
  }

  node_ptr pop_front() noexcept {
    auto old_head = _head;
    _head = _head->next;
    if (_head == nullptr) { _tail = nullptr; }
    --_count;
    return old_head;
  }

  void clear() noexcept {
    _head = nullptr;
    _tail = nullptr;
    _count = 0;
  }

  node_ptr front() const noexcept { return _head; }
  node_ptr back() const noexcept { return _tail; }
  bool empty() const noexcept { return _head == nullptr; }
  size_type size() const noexcept { return _count; }

  // Tail-based operations
  void push_back(node_ptr node) noexcept {
    node->next = nullptr;
    if (_tail == nullptr) {
      _head = _tail = node;
    } else {
      _tail->next = node;
      _tail = node;
    }
    ++_count;
  }

  // O(n) - must traverse to find node before tail
  node_ptr pop_back() noexcept {
    if (_head == _tail) {
      auto old = _tail;
      _head = _tail = nullptr;
      --_count;
      return old;
    }

    auto current = _head;
    while (current->next != _tail) {
      current = current->next;
    }
    auto old_tail = _tail;
    current->next = nullptr;
    _tail = current;
    --_count;
    return old_tail;
  }

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
    friend class intrusive_slist;
    node_ptr _current{nullptr};

  public:
    iterator() = default;
    explicit iterator(node_ptr p) : _current(p) {}

    value_type &dereference() const noexcept { return *_current; }
    void increment() noexcept { _current = _current->next; }
    bool equals(const iterator &other) const noexcept {
      return _current == other._current;
    }
    node_ptr node() const noexcept { return _current; }
  };

  iterator begin() const noexcept {
    // Read _head's raw bytes
    const unsigned char *bytes =
        reinterpret_cast<const unsigned char *>(&_head);
    return iterator(_head);
  }
  iterator end() const noexcept {
    node_ptr null_ptr{nullptr};
    return iterator(null_ptr);
  }

  void insert_after(iterator pos, node_ptr node) noexcept {
    if (pos.node() == nullptr) {
      push_front(node);
    } else {
      node->next = pos.node()->next;
      pos.node()->next = node;
      if (pos.node() == _tail) { _tail = node; }
      ++_count;
    }
  }

  node_ptr erase_after(iterator pos) noexcept {
    if (pos.node() == nullptr || pos.node()->next == nullptr) {
      return nullptr;
    }

    auto to_erase = pos.node()->next;
    pos.node()->next = to_erase->next;
    if (to_erase == _tail) { _tail = pos.node(); }
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
        if (node == _tail) { _tail = current; }
        --_count;
        return true;
      }
      current = current->next;
    }
    return false;
  }
};
