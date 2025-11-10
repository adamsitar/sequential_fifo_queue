#pragma once
#include <cassert>
#include <concepts>
#include <core/intrusive_slist.h>
#include <cstddef>
#include <iterator>
#include <iterators/container_interface.h>
#include <iterators/iterator_facade.h>
#include <result/result.h>
#include <types.h>

template <typename allocator_type> struct offset_list_allocator_storage {
  inline static allocator_type *_allocator{nullptr};
};

// Singly-linked list using segmented pointers
template <is_nothrow T, homogenous allocator_type>
class offset_list
    : public forward_container_iterator_interface<
          offset_list<T, allocator_type>>,
      public before_begin_iterator_interface<offset_list<T, allocator_type>> {
public:
  struct node;
  using node_pointer =
      typename allocator_type::pointer_type::template rebind<node>;
  using storage = offset_list_allocator_storage<allocator_type>;

  struct node {
    node_pointer next;
    T value;
  };

  static_assert(sizeof(node) <= allocator_type::block_size,
                "Allocator block_size is too small for offset_list node");
  static_assert(allocator_type::block_size % alignof(node) == 0,
                "Allocator block_size must be a multiple of node alignment");

private:
  intrusive_slist<node_pointer> _list;
  node_pointer _tail;

public:
  offset_list(const offset_list &) = delete;
  offset_list &operator=(const offset_list &) = delete;
  offset_list(offset_list &&) = delete;
  offset_list &operator=(offset_list &&) = delete;

  explicit offset_list(allocator_type *allocator) : _tail(nullptr) {
    fatal(allocator == nullptr, "Allocator cannot be null");
    storage::_allocator = allocator;
  }

  ~offset_list() { clear(); }

  bool is_empty() const noexcept { return _list.empty(); }
  size_t size() const noexcept { return _list.size(); }

  template <typename U>
    requires std::constructible_from<T, U>
  result<> push_front(U &&value) noexcept {
    auto mem = ok(storage::_allocator->allocate_block());
    auto new_node = new (mem) node{nullptr, T(std::forward<U>(value))};
    node_pointer new_node_ptr(new_node);

    _list.push_front(new_node_ptr);
    if (_tail == nullptr) { _tail = new_node_ptr; }

    return {};
  }

  result<> emplace_front(auto &&...args) noexcept {
    auto mem = ok(storage::_allocator->allocate_block());
    auto new_node = new (mem) node{nullptr, T(exforward(args)...)};
    node_pointer new_node_ptr(new_node);

    _list.push_front(new_node_ptr);
    if (_tail == nullptr) { _tail = new_node_ptr; }

    return {};
  }

  result<T> pop_front() noexcept
    requires std::is_move_constructible_v<T>
  {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _list.pop_front();
    T value = std::move(to_delete->value);

    if (_list.empty()) { _tail = nullptr; }

    to_delete->~node();
    storage::_allocator->deallocate_block(to_delete);

    return value;
  }

  // O(n) if list is more than 1
  result<T> pop_back() noexcept
    requires std::is_move_constructible_v<T>
  {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _tail;
    T value = std::move(to_delete->value);

    if (_list.front() == _tail) {
      _list.pop_front();
      _tail = nullptr;
    } else {
      node *current = _list.front();
      while (current->next != _tail) {
        current = current->next;
      }

      current->next = nullptr;
      _tail = node_pointer(current);
    }

    to_delete->~node();
    storage::_allocator->deallocate_block(to_delete);

    return value;
  }

  // Erase back element without returning it (for non-movable types)
  // O(n) if list is more than 1
  result<> erase_back() noexcept {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _tail;

    if (_list.front() == _tail) {
      _list.pop_front();
      _tail = nullptr;
    } else {
      node *current = _list.front();
      while (current->next != _tail) {
        current = current->next;
      }

      current->next = nullptr;
      _tail = node_pointer(current);
    }

    to_delete->~node();
    storage::_allocator->deallocate_block(to_delete);

    return {};
  }

  result<const T *> front() const noexcept {
    fail(is_empty(), "list empty");
    return &_list.front()->value;
  }

  result<const T *> back() const noexcept {
    fail(is_empty(), "list empty");
    fatal(_tail == nullptr);
    return &_tail->value;
  }

  // traverse, O(n)
  void clear() noexcept {
    while (!_list.empty()) {
      node_pointer to_delete = _list.pop_front();
      to_delete->~node();
      storage::_allocator->deallocate_block(to_delete);
    }
    _tail = nullptr;
  }

  class iterator;
  iterator before_begin() noexcept { return iterator(this, nullptr, true); }
  iterator begin() const noexcept {
    return iterator(this, static_cast<node *>(_list.front()), false);
  }
  iterator end() const noexcept { return iterator(this, nullptr, false); }
  // cbefore_begin, cbegin, cend provided by container_iterator_interface
  iterator insert_after(iterator pos, auto &&value) noexcept
    requires std::constructible_from<T, decltype(value)>;
  iterator emplace_after(iterator pos, auto &&...args) noexcept;
  iterator erase_after(iterator pos) noexcept;
  iterator erase_after(iterator pos, iterator last) noexcept;
};

template <is_nothrow T, homogenous allocator_type>
class offset_list<T, allocator_type>::iterator
    : public forward_iterator_facade<T> {
  friend class offset_list;

  const offset_list *_list;
  node *_current;
  bool _is_before_begin;

  iterator(const offset_list *list, node *current, bool is_before_begin)
      : _list(list), _current(current), _is_before_begin(is_before_begin) {}

public:
  // CRTP primitives for forward_iterator_facade
  T &dereference() const noexcept {
    fatal(_is_before_begin, "Cannot dereference before_begin iterator");
    fatal(_current == nullptr, "Cannot dereference end iterator");
    return _current->value;
  }

  void increment() noexcept {
    if (_is_before_begin) {
      _is_before_begin = false;
      _current = static_cast<node *>(_list->_list.front());
    } else {
      fatal(_current == nullptr, "Cannot increment end iterator");
      _current = static_cast<node *>(_current->next);
    }
  }

  bool equals(const iterator &other) const noexcept {
    return _current == other._current &&
           _is_before_begin == other._is_before_begin;
  }

  // Accessors for implementation details
  node *get_node() const noexcept { return _current; }
  bool is_before_begin() const noexcept { return _is_before_begin; }
};

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::insert_after(iterator pos,
                                             auto &&value) noexcept
  requires std::constructible_from<T, decltype(value)>
{
  auto mem = unwrap(storage::_allocator->allocate_block());
  node *new_node = new (mem) node{nullptr, T(exforward(value))};
  node_pointer new_node_ptr(new_node);

  if (pos._is_before_begin) {
    _list.push_front(new_node_ptr);
    if (_tail == nullptr) { _tail = new_node_ptr; }
  } else {
    fatal(pos._current == nullptr, "Cannot insert_after at end() position");
    _list.insert_after(pos, new_node);
    if (pos._current == _tail) { _tail = new_node_ptr; }
  }

  return iterator(this, new_node, false);
}

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::emplace_after(iterator pos,
                                              auto &&...args) noexcept {
  auto mem = unwrap(storage::_allocator->allocate_block());
  node *new_node = new (mem) node{nullptr, T(exforward(args)...)};
  node_pointer new_node_ptr(new_node);

  if (pos._is_before_begin) {
    _list.push_front(new_node_ptr);
    if (_tail == nullptr) { _tail = new_node_ptr; }
  } else {
    fatal(pos._current != nullptr, "Cannot emplace_after at end() position");
    new_node_ptr->next = pos._current->next;
    pos._current->next = new_node_ptr;
    if (pos._current == _tail) { _tail = new_node_ptr; }
  }

  return iterator(this, new_node, false);
}

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::erase_after(iterator pos) noexcept {
  node_pointer to_erase_ptr;
  node *to_erase;

  if (pos._is_before_begin) {
    if (_list.empty()) { return end(); }
    to_erase_ptr = _list.pop_front();
    to_erase = static_cast<node *>(to_erase_ptr);
  } else {
    fatal(pos._current == nullptr, "Cannot erase_after at end() position");
    if (pos._current->next == nullptr) { return end(); }
    to_erase = _list.erase_after(pos);
  }

  node *next_node = static_cast<node *>(to_erase->next);

  if (to_erase_ptr == _tail) {
    _tail = _list.empty() ? nullptr : node_pointer(pos._current);
  }

  to_erase->~node();
  storage::_allocator->deallocate_block(to_erase_ptr);

  return iterator(this, next_node, false);
}

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::erase_after(iterator pos,
                                            iterator last) noexcept {
  iterator current = pos;
  ++current;
  while (current != last) {
    current = erase_after(pos);
  }

  return last;
}
