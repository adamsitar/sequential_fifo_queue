#pragma once
#include <array>
#include <cstdint>
#include <print>
#include <ptr_utils.h>
#include <ranges>
#include <result/result.h>
#include <type_traits>
#include <types.h>

template <size_t size, typename offset_type>
  requires nonzero_power_of_two<size>
union block {
  offset_type next_offset;
  std::array<std::byte, size> data;

  block(const std::array<std::byte, size> &arr) : data(arr) {}
  block() : data{} {}
};

template <std::size_t block_size, std::size_t block_count>
  requires nonzero_power_of_two<block_size, block_count>
class freelist_storage {
public:
  using offset_type = smallest_t<block_count>;
  using block_type = std::array<std::byte, block_size>;

  // Null sentinel is max value of offset_type (since offset_type is now a primitive)
  static constexpr offset_type null_sentinel = std::numeric_limits<offset_type>::max();

  freelist_storage(offset_type &head, offset_type &count) {
    reset(head, count);
  }
  freelist_storage() = delete;

private:
  using internal_block_type = block<block_size, offset_type>;
  std::array<internal_block_type, block_count> _storage;

  auto get_by_offset(this auto &&self, offset_type offset)
      -> result<decltype((self._storage[offset].data))> {
    fail(offset >= block_count);
    return {exforward(self)._storage[offset].data};
  }

  offset_type offset_of(const block_type &elem) const {
    return ptr::element_index(_storage.data(), &elem);
  }

  void insert(block_type &elem, offset_type &head, offset_type &count) {
    offset_type elem_offset = offset_of(elem);
    _storage[elem_offset].next_offset = head;
    head = elem_offset;
    count++;
  }

public:
  std::byte *base() const noexcept {
    return reinterpret_cast<std::byte *>(
        const_cast<internal_block_type *>(_storage.data()));
  }

  constexpr size_t size() const noexcept { return _storage.size(); }
  constexpr size_t max_size() const noexcept { return _storage.max_size(); }
  bool is_full(size_t count) const noexcept { return count >= max_size(); }
  bool is_empty(offset_type head) const noexcept {
    return head == null_sentinel;
  }

  bool owns(block_type &elem) const noexcept {
    return ptr::contains_bytes(_storage.data(), sizeof(_storage), &elem);
  }

  void reset(offset_type &head, offset_type &count) {
    head = null_sentinel;
    count = 0;
    for (auto &current : std::ranges::reverse_view{_storage}) {
      insert(current.data, head, count);
    }
  }

  result<const block_type &> head(offset_type head_offset) const {
    fail(head_offset == null_sentinel, "list empty");
    return get_by_offset(head_offset);
  }

  result<block_type &> pop(offset_type &head, offset_type &count) {
    fail(head == null_sentinel, "list empty").stacktrace();

    auto &head_block = ok(get_by_offset(head));
    head = _storage[head].next_offset;
    count--;

    return head_block;
  }

  result<> push(block_type &elem, offset_type &head, offset_type &count) {
    fail(count >= max_size(), "list full").stacktrace();
    fail(!owns(elem), "invalid pointer");

    insert(elem, head, count);
    return {};
  }
};

template <std::size_t block_size, std::size_t block_count, typename unique_tag>
  requires nonzero_power_of_two<block_size, block_count>
class freelist {
public:
  using offset_type = smallest_t<block_count>;
  using block_type = std::array<std::byte, block_size>;

  // Null sentinel is max value of offset_type (since offset_type is now a primitive)
  static constexpr offset_type null_sentinel = std::numeric_limits<offset_type>::max();

private:
  offset_type _head{null_sentinel};
  offset_type _count{0};
  alignas(block_size) freelist_storage<block_size, block_count> _storage;

public:
  std::byte *base() const noexcept { return _storage.base(); }
  constexpr size_t size() const noexcept { return _storage.size(); }
  constexpr size_t max_size() const noexcept { return _storage.max_size(); }
  bool is_full() const noexcept { return _storage.is_full(_count); }
  bool is_empty() const noexcept { return _storage.is_empty(_head); }

  bool owns(const block_type &elem) const noexcept {
    return _storage.owns(elem);
  }

  freelist() : _storage(_head, _count) {}
  ~freelist() = default;

  void reset() { _storage.reset(_head, _count); }
  result<const block_type &> head() const { return _storage.head(_head); }
  result<block_type &> pop() { return _storage.pop(_head, _count); }
  result<> push(block_type &elem) { return _storage.push(elem, _head, _count); }
};
