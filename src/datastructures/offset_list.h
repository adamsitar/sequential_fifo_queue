#pragma once
#include <allocators/test_allocator.h>
#include <array>
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
template <is_nothrow T, is_homogenous allocator_type = simple_test_allocator>
class offset_list
    : public forward_iterator_interface<offset_list<T, allocator_type>>,
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
                "node must be smaller or equal to block size");
  static_assert(allocator_type::block_size % alignof(node) == 0,
                "Allocator block_size must be a multiple of node alignment");

  intrusive_slist<node_pointer> _list;

private:
  node_pointer allocate_node(auto &&...args) noexcept {
    auto mem = unwrap(storage::_allocator->allocate_block());
    void *raw_ptr = static_cast<void *>(mem);
    node *new_node =
        new (mem) node{node_pointer(nullptr), T(exforward(args)...)};
    node_pointer result(static_cast<void *>(mem));
    return result;
  }

  void deallocate_node(node_pointer ptr) noexcept {
    ptr->~node();
    storage::_allocator->deallocate_block(ptr);
  }

private:
  // Static default allocator instance for default-constructed containers
  inline static allocator_type default_allocator{};

public:
  offset_list(const offset_list &) = delete;
  offset_list &operator=(const offset_list &) = delete;
  offset_list(offset_list &&) = delete;
  offset_list &operator=(offset_list &&) = delete;

  // Default constructor - uses static test allocator
  offset_list() : offset_list(&default_allocator) {}

  explicit offset_list(allocator_type *allocator) {
    fatal(allocator == nullptr, "Allocator cannot be null");
    storage::_allocator = allocator;
  }

  ~offset_list() {
    clear();
    // Clear static storage to prevent stale allocator pointers
    storage::_allocator = nullptr;
  }

  bool is_empty() const noexcept { return _list.empty(); }
  size_t size() const noexcept { return _list.size(); }

  template <typename U>
    requires std::constructible_from<T, U>
  result<> push_front(U &&value) noexcept {
    node_pointer new_node = allocate_node(std::forward<U>(value));
    _list.push_front(new_node);
    return {};
  }

  result<> emplace_front(auto &&...args) noexcept {
    node_pointer new_node = allocate_node(exforward(args)...);
    _list.push_front(new_node);
    return {};
  }

  result<T> pop_front() noexcept
    requires std::is_move_constructible_v<T>
  {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _list.pop_front();
    T value = std::move(to_delete->value);
    deallocate_node(to_delete);

    return value;
  }

  // O(n) - must traverse to find node before tail
  result<T> pop_back() noexcept
    requires std::is_move_constructible_v<T>
  {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _list.pop_back();
    T value = std::move(to_delete->value);
    deallocate_node(to_delete);

    return value;
  }

  // Erase back element without returning it (for non-movable types)
  // O(n) - must traverse to find node before tail
  result<> erase_back() noexcept {
    fail(is_empty(), "list empty");

    node_pointer to_delete = _list.pop_back();
    deallocate_node(to_delete);

    return {};
  }

  result<const T *> front() const noexcept {
    fail(is_empty(), "list empty");
    return &_list.front()->value;
  }

  result<const T *> back() const noexcept {
    fail(is_empty(), "list empty");
    return &_list.back()->value;
  }

  // traverse, O(n)
  void clear() noexcept {
    while (!_list.empty()) {
      deallocate_node(_list.pop_front());
    }
  }

  class iterator;
  iterator before_begin() noexcept { return iterator(this, true); }
  iterator begin() const noexcept {
    return iterator(this, _list.begin(), false);
  }
  iterator end() const noexcept { return iterator(this, _list.end(), false); }
  // cbefore_begin, cbegin, cend provided by container_iterator_interface
  iterator insert_after(iterator pos, auto &&value) noexcept
    requires std::constructible_from<T, decltype(value)>;
  iterator emplace_after(iterator pos, auto &&...args) noexcept;
  iterator erase_after(iterator pos) noexcept;
  iterator erase_after(iterator pos, iterator last) noexcept;
};

template <is_nothrow T, is_homogenous allocator_type>
struct offset_list<T, allocator_type>::iterator
    : public forward_iterator_facade<T> {
  friend class offset_list;
  using intrusive_iterator = typename intrusive_slist<node_pointer>::iterator;
  const offset_list *_list{nullptr};
  intrusive_iterator _intrusive_it{nullptr};
  bool _is_before_begin{false};

  iterator(const offset_list *list, intrusive_iterator it, bool is_before_begin)
      : _list(list), _intrusive_it(it), _is_before_begin(is_before_begin) {}

  // Constructor for before_begin
  iterator(const offset_list *list, bool is_before_begin)
      : _list(list), _intrusive_it(_list->_list.end()),
        _is_before_begin(is_before_begin) {}

public:
  // CRTP primitives for forward_iterator_facade
  T &dereference() const noexcept {
    fatal(_is_before_begin, "Cannot dereference before_begin iterator");
    auto node_ptr = _intrusive_it.node();
    return node_ptr->value;
  }

  void increment() noexcept {
    if (_is_before_begin) {
      _is_before_begin = false;
      _intrusive_it = _list->_list.begin();
    } else {
      ++_intrusive_it;
    }
  }

  bool equals(const iterator &other) const noexcept {
    return _intrusive_it == other._intrusive_it &&
           _is_before_begin == other._is_before_begin;
  }

  intrusive_iterator intrusive() const { return _intrusive_it; }
  bool is_before_begin() const { return _is_before_begin; }
};

template <is_nothrow T, is_homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::insert_after(iterator pos,
                                             auto &&value) noexcept
  requires std::constructible_from<T, decltype(value)>
{
  node_pointer new_node = allocate_node(exforward(value));

  if (pos._is_before_begin) {
    _list.push_front(new_node);
    return iterator(this, _list.begin(), false);
  }

  fatal(pos._intrusive_it.node() == nullptr,
        "Cannot insert_after at end() position");

  _list.insert_after(pos._intrusive_it, new_node);
  auto result_it = pos._intrusive_it;
  ++result_it;

  return iterator(this, result_it, false);
}

template <is_nothrow T, is_homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::emplace_after(iterator pos,
                                              auto &&...args) noexcept {
  node_pointer new_node = allocate_node(exforward(args)...);

  if (pos._is_before_begin) {
    _list.push_front(new_node);
    return iterator(this, _list.begin(), false);
  }

  fatal(pos._intrusive_it.node() == nullptr,
        "Cannot emplace_after at end() position");

  _list.insert_after(pos._intrusive_it, new_node);
  auto result_it = pos._intrusive_it;
  ++result_it;

  return iterator(this, result_it, false);
}

template <is_nothrow T, is_homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::erase_after(iterator pos) noexcept {
  if (pos._is_before_begin) {
    if (_list.empty()) { return end(); }
    deallocate_node(_list.pop_front());
    return begin();
  }

  fatal(pos._intrusive_it.node() == nullptr,
        "Cannot erase_after at end() position");

  if (pos._intrusive_it.node()->next == nullptr) { return end(); }

  deallocate_node(_list.erase_after(pos._intrusive_it));
  auto result_it = pos._intrusive_it;
  ++result_it;

  return iterator(this, result_it, false);
}

template <is_nothrow T, is_homogenous allocator_type>
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
