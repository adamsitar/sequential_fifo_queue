#pragma once
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <result.h>
#include <segmented_ptr.h>
#include <types.h>

template <typename allocator_type> struct offset_list_allocator_storage {
  inline static allocator_type *_allocator{nullptr};
};

// Singly-linked list using segmented pointers
template <is_nothrow T, homogenous allocator_type> class offset_list {
public:
  struct node;
  using node_pointer =
      basic_segmented_ptr<node, typename allocator_type::offset_type,
                          typename allocator_type::unique_tag, uint8_t>;
  using storage = offset_list_allocator_storage<allocator_type>;

  struct node {
    node_pointer next;
    T value;
    explicit node(auto &&val)
        : next(nullptr), value(std::forward<decltype(val)>(val)) {}
  };

  static_assert(sizeof(node) <= allocator_type::block_size_v,
                "Allocator block_size is too small for offset_list node");
  static_assert(allocator_type::block_size_v % alignof(node) == 0,
                "Allocator block_size must be a multiple of node alignment");

private:
  node_pointer _head;
  node_pointer _tail;

public:
  offset_list(const offset_list &) = delete;
  offset_list &operator=(const offset_list &) = delete;
  offset_list(offset_list &&) = delete;
  offset_list &operator=(offset_list &&) = delete;

  explicit offset_list(allocator_type *allocator)
      : _head(nullptr), _tail(nullptr) {
    fatal(allocator == nullptr, "Allocator cannot be null");

    // Always update static allocator (handles allocator replacement)
    storage::_allocator = allocator;
  }

  ~offset_list() { clear(); }

  bool is_empty() const noexcept { return _head == nullptr; }

  size_t size() const noexcept {
    size_t count = 0;
    for (auto current = _head; current != nullptr; current = current->next) {
      ++count;
    }
    return count;
  }

  template <typename U>
    requires std::constructible_from<T, U>
  result<> push_front(U &&value) noexcept {
    auto mem = TRY(storage::_allocator->allocate_block());
    node *new_node = new (mem) node(std::forward<U>(value));
    node_pointer new_node_ptr(new_node);

    new_node_ptr->next = _head;
    _head = new_node_ptr;

    // Update tail if list was empty
    if (_tail == nullptr) {
      _tail = new_node_ptr;
    }

    return {};
  }

  result<> emplace_front(auto &&...args) noexcept {
    auto mem = TRY(storage::_allocator->allocate_block());

    void *raw_mem = static_cast<void *>(mem);
    node *new_node = static_cast<node *>(raw_mem);
    new_node->next = nullptr;
    new (&new_node->value) T(std::forward<decltype(args)>(args)...);

    node_pointer new_node_ptr(new_node); // Cache hit!

    new_node_ptr->next = _head;
    _head = new_node_ptr;

    // Update tail if list was empty
    if (_tail == nullptr) {
      _tail = new_node_ptr;
    }

    return {};
  }

  result<T> pop_front() noexcept {
    fail(is_empty(), "list empty");

    // Extract segment_id before doing anything else
    auto segment_id = _head.get_segment_id();

    // Get raw pointer to node
    node *front_node = static_cast<node *>(_head);
    fatal(front_node == nullptr);

    // Move value out before destroying node
    T value = std::move(front_node->value);

    // Update head (must be done before destruction in case next is within the
    // node)
    _head = front_node->next;

    // Update tail if list is now empty
    if (_head == nullptr) {
      _tail = nullptr;
    }

    // Destroy the node
    front_node->~node();

    // Convert to allocator's pointer type for deallocation
    using block_type = typename allocator_type::block_type;
    auto *block_raw = reinterpret_cast<block_type *>(front_node);
    typename allocator_type::pointer_type alloc_ptr(segment_id, block_raw);

    // Deallocate block
    storage::_allocator->deallocate_block(alloc_ptr);

    return value;
  }

  result<T> pop_back() noexcept {
    fail(is_empty(), "list empty");

    // Extract segment_id before doing anything else
    auto segment_id = _tail.get_segment_id();

    // Get raw pointer to tail node
    node *back_node = static_cast<node *>(_tail);
    fatal(back_node == nullptr);

    // Move value out before destroying node
    T value = std::move(back_node->value);

    // Find second-to-last node (need to traverse - O(n) for singly-linked list)
    if (_head == _tail) {
      // Only one node
      _head = nullptr;
      _tail = nullptr;
    } else {
      // Multiple nodes - traverse to find second-to-last
      node *current = static_cast<node *>(_head);
      while (current->next != _tail) {
        current = static_cast<node *>(current->next);
      }
      current->next = nullptr;
      _tail = node_pointer(current);
    }

    // Destroy the node
    back_node->~node();

    // Convert to allocator's pointer type for deallocation
    using block_type = typename allocator_type::block_type;
    auto *block_raw = reinterpret_cast<block_type *>(back_node);
    typename allocator_type::pointer_type alloc_ptr(segment_id, block_raw);

    // Deallocate block
    storage::_allocator->deallocate_block(alloc_ptr);

    return value;
  }

  result<const T *> front() const noexcept {
    fail(is_empty(), "list empty");
    node *front_node = static_cast<node *>(_head);
    fatal(front_node == nullptr);
    return &front_node->value;
  }

  result<const T *> back() const noexcept {
    fail(is_empty(), "list empty");
    node *back_node = static_cast<node *>(_tail);
    fatal(back_node == nullptr);
    return &back_node->value;
  }

  void clear() noexcept {
    while (!is_empty()) {
      // Extract segment_id before modifying anything
      auto segment_id = _head.get_segment_id();

      // Get raw pointer to current node
      node *current = static_cast<node *>(_head);

      // Update head
      _head = current->next;

      // Destroy the node
      current->~node();

      // Convert to allocator's pointer type for deallocation
      using block_type = typename allocator_type::block_type;
      auto *block_raw = reinterpret_cast<block_type *>(current);
      typename allocator_type::pointer_type alloc_ptr(segment_id, block_raw);

      // Deallocate block
      storage::_allocator->deallocate_block(alloc_ptr);
    }

    // Ensure tail is null when list is empty
    _tail = nullptr;
  }

  // ============================================================================
  // Iterator Support
  // ============================================================================

  class iterator;

  /**
   * @brief Get iterator to position before first element.
   * @time O(1)
   *
   * This special iterator allows uniform handling of insertions/erasures
   * at the head using the _after operations. Cannot be dereferenced.
   */
  iterator before_begin() noexcept { return iterator(this, nullptr, true); }
  iterator cbefore_begin() const noexcept {
    return iterator(this, nullptr, true);
  }

  iterator begin() const noexcept {
    return iterator(this, static_cast<node *>(_head), false);
  }

  iterator end() const noexcept { return iterator(this, nullptr, false); }

  // ============================================================================
  // Singly-Linked List Primitives
  // ============================================================================

  /**
   * @brief Insert element after the given position.
   * @param pos Iterator to position after which to insert
   * @param value Value to insert (forwarded)
   * @return Iterator to the inserted element, or end() if allocation fails
   * @time O(1)
   */
  iterator insert_after(iterator pos, auto &&value) noexcept
    requires std::constructible_from<T, decltype(value)>;

  /**
   * @brief Construct element in-place after the given position.
   * @param pos Iterator to position after which to insert
   * @param args Constructor arguments for T
   * @return Iterator to the inserted element, or end() if allocation fails
   * @time O(1)
   */
  iterator emplace_after(iterator pos, auto &&...args) noexcept;

  /**
   * @brief Erase the element after the given position.
   * @param pos Iterator to position before the element to erase
   * @return Iterator to the element after the erased one
   * @time O(1)
   *
   * Example:
   *   auto it = list.before_begin();
   *   list.erase_after(it); // Erases the first element
   */
  iterator erase_after(iterator pos) noexcept;

  /**
   * @brief Erase elements in range (pos, last).
   * @param pos Iterator to position before first element to erase
   * @param last Iterator to position after last element to erase
   * @return Iterator to last (the element after the erased range)
   * @time O(n) where n is the number of elements erased
   */
  iterator erase_after(iterator pos, iterator last) noexcept;
};

/**
 * @brief Forward iterator for offset_list.
 *
 * This iterator provides read-only traversal through the linked list.
 * It maintains a pointer to the parent list for segment-to-pointer conversion,
 * a pointer to the current node, and a flag indicating if this is a
 * before_begin iterator.
 */
template <is_nothrow T, homogenous allocator_type>
class offset_list<T, allocator_type>::iterator {
  friend class offset_list;

  const offset_list *_list;
  node *_current;
  bool _is_before_begin;

  iterator(const offset_list *list, node *current, bool is_before_begin)
      : _list(list), _current(current), _is_before_begin(is_before_begin) {}

public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using reference = T &;

  reference operator*() const noexcept {
    fatal(_is_before_begin, "Cannot dereference before_begin iterator");
    fatal(_current == nullptr, "Cannot dereference end iterator");
    return _current->value;
  }

  pointer operator->() const noexcept {
    fatal(_is_before_begin, "Cannot dereference before_begin iterator");
    fatal(_current == nullptr, "Cannot dereference end iterator");
    return &_current->value;
  }

  iterator &operator++() noexcept {
    if (_is_before_begin) {
      // Incrementing before_begin yields begin()
      _is_before_begin = false;
      _current = static_cast<node *>(_list->_head);
    } else {
      fatal(_current == nullptr, "Cannot increment end iterator");
      _current = static_cast<node *>(_current->next);
    }
    return *this;
  }

  iterator operator++(int) noexcept {
    iterator tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const iterator &other) const noexcept {
    return _current == other._current &&
           _is_before_begin == other._is_before_begin;
  }

  bool operator!=(const iterator &other) const noexcept {
    return !(*this == other);
  }

  // Allow access to underlying node for advanced operations
  node *get_node() const noexcept { return _current; }

  // Public accessors for offset_list methods (allows inline implementations)
  bool is_before_begin() const noexcept { return _is_before_begin; }
};

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::insert_after(iterator pos,
                                             auto &&value) noexcept
  requires std::constructible_from<T, decltype(value)>
{
  // Allocate memory for new node (returns allocator::pointer_type)
  auto mem_result = storage::_allocator->allocate_block();
  if (!mem_result) {
    return end();
  }
  auto mem = *mem_result;

  // Convert allocator's pointer to raw pointer for placement new
  void *raw_mem = static_cast<void *>(mem);

  // Construct node in allocated memory
  node *new_node = new (raw_mem) node(std::forward<decltype(value)>(value));

  // Create node_pointer - cache will hit alloc_cache (O(1))
  node_pointer new_node_ptr(new_node);

  if (pos._is_before_begin) {
    // Inserting after before_begin means inserting at head
    new_node_ptr->next = _head;
    _head = new_node_ptr;
  } else {
    fatal(pos._current == nullptr, "Cannot insert_after at end() position");
    new_node_ptr->next = pos._current->next;
    pos._current->next = new_node_ptr;
  }

  return iterator(this, new_node, false);
}

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::emplace_after(iterator pos,
                                              auto &&...args) noexcept {
  // Allocate memory for new node (returns allocator::pointer_type)
  auto mem_result = storage::_allocator->allocate_block();
  if (!mem_result) {
    return end();
  }
  auto mem = *mem_result;

  // Convert allocator's pointer to raw pointer for placement new
  void *raw_mem = static_cast<void *>(mem);

  // Construct node in allocated memory
  node *new_node = static_cast<node *>(raw_mem);
  new_node->next = nullptr;
  new (&new_node->value) T(std::forward<decltype(args)>(args)...);

  // Create node_pointer - cache will hit alloc_cache (O(1))
  node_pointer new_node_ptr(new_node);

  if (pos._is_before_begin) {
    new_node_ptr->next = _head;
    _head = new_node_ptr;
  } else {
    fatal(pos._current != nullptr, "Cannot emplace_after at end() position");
    new_node_ptr->next = pos._current->next;
    pos._current->next = new_node_ptr;
  }

  return iterator(this, new_node, false);
}

template <is_nothrow T, homogenous allocator_type>
typename offset_list<T, allocator_type>::iterator
offset_list<T, allocator_type>::erase_after(iterator pos) noexcept {
  node_pointer to_erase_ptr;
  node *to_erase;

  if (pos._is_before_begin) {
    // Erasing after before_begin means erasing head
    if (_head == nullptr)
      return end();

    to_erase_ptr = _head;
    to_erase = static_cast<node *>(_head);
    _head = to_erase->next;
  } else {
    fatal(pos._current == nullptr, "Cannot erase_after at end() position");
    if (pos._current->next == nullptr)
      return end();

    to_erase_ptr = pos._current->next;
    to_erase = static_cast<node *>(pos._current->next);
    pos._current->next = to_erase->next;
  }

  // Extract segment_id for efficient deallocation
  auto segment_id = to_erase_ptr.get_segment_id();

  // Save next node before destruction
  node *next_node = static_cast<node *>(to_erase->next);

  // Destroy the node
  to_erase->~node();

  // Convert to allocator's pointer type for deallocation
  using block_type = typename allocator_type::block_type;
  auto *block_raw = reinterpret_cast<block_type *>(to_erase);
  typename allocator_type::pointer_type alloc_ptr(segment_id, block_raw);

  // Deallocate block
  storage::_allocator->deallocate_block(alloc_ptr);

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
