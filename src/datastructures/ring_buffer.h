#pragma once
#include <compare>
#include <cstddef>
#include <iterators/container_interface.h>
#include <iterators/iterator_facade.h>
#include <types.h>

template <typename allocator_type> struct ring_buffer_allocator_storage {
  inline static allocator_type *_allocator{nullptr};
};

// Fixed-capacity circular buffer
template <is_nothrow T, std::size_t count, homogenous allocator_type>
class ring_buffer
    : public bidirectional_container_iterator_interface<
          ring_buffer<T, count, allocator_type>> {
public:
  using value_type = T;
  using size_type = smallest_t<count>;
  using storage = ring_buffer_allocator_storage<allocator_type>;

  static constexpr std::size_t capacity_v = count;
  static constexpr std::size_t storage_bytes_v = count * sizeof(T);

  static_assert(count > 0, "ring_buffer count must be > 0");
  static_assert(sizeof(T) > 0, "T must be a complete type");

  // accessors to storage, with respect to lifetime rules
  constexpr void *construction_location(size_type index) const noexcept {
    auto *base = static_cast<std::byte *>(static_cast<void *>(_storage));
    return base + sizeof(T) * index;
  }

  constexpr T *storage_ptr() const noexcept {
    return static_cast<T *>(static_cast<void *>(_storage));
  }

private:
  size_type _head{0};     // Index of first element (oldest)
  size_type _tail{0};     // Index of next empty slot
  size_type _free{count}; // Number of free slots
  allocator_type::pointer_type _storage;

  constexpr void advance_tail() noexcept {
    _tail = (_tail + 1) % count;
    --_free;
  }

  constexpr void advance_head() noexcept {
    _head = (_head + 1) % count;
    ++_free;
  }

public:
  explicit ring_buffer(allocator_type *alloc) {
    fatal(alloc == nullptr, "Allocator cannot be null");

    // Always update static allocator (handles allocator replacement)
    storage::_allocator = alloc;

    auto result = storage::_allocator->allocate_block();
    fatal(!result, "Failed to allocate ring_buffer storage");
    _storage = *result;
  }

  ~ring_buffer() {
    clear();

    if (storage::_allocator != nullptr && _storage != nullptr) {
      storage::_allocator->deallocate_block(_storage);
    }
  }

  ring_buffer(const ring_buffer &other,
              allocator_type *alloc = nullptr) noexcept
      : _head(other._head), _tail(other._tail), _free(other._free) {
    // Update static allocator if provided
    if (alloc != nullptr) {
      storage::_allocator = alloc;
    }

    fatal(storage::_allocator == nullptr, "Allocator not set");

    auto result = storage::_allocator->allocate_block();
    fatal(!result, "Failed to allocate ring_buffer storage");
    _storage = *result;

    // TODO: If needed, implement element-by-element copy
  }

  ring_buffer &operator=(const ring_buffer &) = delete;
  ring_buffer(ring_buffer &&) noexcept = delete;
  ring_buffer &operator=(ring_buffer &&) noexcept = delete;

  void clear() noexcept {
    while (!empty()) {
      (storage_ptr() + _head)->~T();
      advance_head();
    }
  }

  // Push element to the back (tail) of the ring buffer.
  template <typename U>
    requires std::same_as<std::remove_cvref_t<U>, T>
  constexpr result<> push(U &&value) {
    fail(is_full(), "Cannot push to full ring_buffer");
    new (construction_location(_tail)) T(std::forward<U>(value));
    advance_tail();
    return {};
  }

  // Construct element in-place at the back.
  template <typename... Args> constexpr T &emplace(Args &&...args) {
    fatal(is_full(), "Cannot emplace in full ring_buffer");
    T *ptr = new (construction_location(_tail)) T(std::forward<Args>(args)...);
    advance_tail();
    return *ptr;
  }

  // Pop element from the front (head) of the ring buffer.
  constexpr result<T> pop() {
    fail(empty(), "Cannot pop from empty ring_buffer");
    T *ptr = storage_ptr();
    T value = std::move(*(ptr + _head));
    (ptr + _head)->~T();
    advance_head();
    return value;
  }

  // Access front element (oldest).
  auto &&front(this auto &&self) {
    fatal(self.empty(), "front() called on empty ring_buffer");
    return *(self.storage_ptr() + self._head);
  }

  // Access back element (newest).
  auto &&back(this auto &&self) {
    fatal(self.empty(), "back() called on empty ring_buffer");
    auto back_pos = (self._tail == 0) ? (count - 1) : (self._tail - 1);
    return *(self.storage_ptr() + back_pos);
  }

  // Access element at logical index (no bounds checking).
  auto &&operator[](this auto &&self, size_type index) {
    return *(self.storage_ptr() + ((self._head + index) % count));
  }

  // Access element at logical index (with bounds checking).
  auto &&at(this auto &&self, size_type index) noexcept {
    if (index >= self.size()) {
      fatal("ring_buffer::at: index out of range");
    }
    return self[index];
  }

  bool is_full() const noexcept { return _free == 0; }
  bool empty() const noexcept { return _free == count; }
  size_type size() const noexcept { return count - _free; }
  size_type capacity() const noexcept { return count; }
  size_type get_free() const noexcept { return _free; }

  class iterator;

  iterator begin() noexcept { return iterator(this, _head); }
  iterator end() noexcept { return iterator(this, _tail); }
  // cbegin/cend/rbegin/rend/crbegin/crend provided by container_iterator_interface
};

// Random access iterator for ring_buffer.
template <is_nothrow T, std::size_t count, homogenous allocator_type>
class ring_buffer<T, count, allocator_type>::iterator
    : public random_access_iterator_facade<T> {
public:
  using base = random_access_iterator_facade<T>;
  using typename base::difference_type;
  using size_type = typename ring_buffer::size_type;

  iterator() noexcept = default;

  iterator(ring_buffer *buffer, size_type pos) noexcept
      : _buffer(buffer), _pos(pos) {}

  // CRTP primitives for random_access_iterator_facade
  T &dereference() const noexcept { return *(_buffer->storage_ptr() + _pos); }

  void advance(difference_type n) noexcept {
    auto new_pos = (static_cast<difference_type>(_pos) + n);
    new_pos %= static_cast<difference_type>(count);
    if (new_pos < 0) {
      new_pos += count;
    }
    _pos = static_cast<size_type>(new_pos);
  }

  difference_type distance_to(const iterator &other) const noexcept {
    return other.rank() - rank();
  }

  bool equals(const iterator &other) const noexcept {
    return _pos == other._pos && _buffer == other._buffer;
  }

private:
  /**
   * @brief Calculate logical position relative to begin().
   * Essential for proper comparison in circular buffers.
   */
  [[nodiscard]] constexpr difference_type rank() const noexcept {
    if (_buffer == nullptr) {
      return 0;
    }
    return (_pos - _buffer->_head + count) % count;
  }

  ring_buffer *_buffer = nullptr;
  size_type _pos = 0;
};
