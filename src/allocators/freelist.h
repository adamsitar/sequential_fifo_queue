#pragma once
#include <array>
#include <cstdint>
#include <ranges>
#include <result.h>
#include <type_traits>
#include <types.h>

template <size_t size, typename offset_type>
  requires nonzero_power_of_two<size>
union block_storage {
  offset_type next_offset; // Index of next block in the freelist
  std::array<std::byte, size> data;
};

template <std::size_t block_size, std::size_t block_count>
  requires nonzero_power_of_two<block_size, block_count>
class freelist_storage {
public:
  using offset_type = smallest_t<block_count>;
  using block_type = std::array<std::byte, block_size>;

private:
  using internal_block_type = block_storage<block_size, offset_type>;
  static constexpr offset_type null_offset =
      std::numeric_limits<offset_type>::max();

  std::array<internal_block_type, block_count> _storage;

  result<internal_block_type *> offset_to_ptr(offset_type offset) {
    if (offset == null_offset) {
      return nullptr;
    }
    fail(offset >= block_count);
    return &_storage[offset];
  }

  offset_type ptr_to_offset(const internal_block_type *ptr) const {
    if (ptr == nullptr) {
      return null_offset;
    }
    return static_cast<offset_type>(ptr - _storage.data());
  }

  void insert(internal_block_type &elem, offset_type &head,
              offset_type &count) {
    elem.next_offset = head;
    head = ptr_to_offset(&elem);
    count++;
  }

public:
  std::byte *base() noexcept {
    return reinterpret_cast<std::byte *>(_storage.data());
  }

  const std::byte *base() const noexcept {
    return reinterpret_cast<const std::byte *>(_storage.data());
  }

  constexpr size_t size() const noexcept { return _storage.size(); }
  constexpr size_t max_size() const noexcept { return _storage.max_size(); }
  bool is_full(size_t count) const noexcept { return count >= max_size(); }
  bool is_empty(offset_type head) const noexcept { return head == null_offset; }

  bool owns(const block_type &elem) const noexcept {
    auto elem_addr = reinterpret_cast<std::uintptr_t>(&elem);
    auto base_addr = reinterpret_cast<std::uintptr_t>(_storage.data());
    auto end_addr =
        reinterpret_cast<std::uintptr_t>(_storage.data() + _storage.size());
    return elem_addr >= base_addr && elem_addr < end_addr;
  }

  void reset(offset_type &head, offset_type &count) {
    head = null_offset;
    count = 0;
    for (auto &current : std::ranges::reverse_view{_storage}) {
      insert(current, head, count);
    }
  }

  result<const block_type *> head(offset_type head_offset) const {
    fail(head_offset == null_offset, "list empty");
    auto *ptr =
        TRY(const_cast<freelist_storage *>(this)->offset_to_ptr(head_offset));
    return reinterpret_cast<const block_type *>(ptr);
  }

  result<block_type *> pop(offset_type &head, offset_type &count) {
    fail(head == null_offset, "list empty");

    auto *tmp_head = TRY(offset_to_ptr(head));
    head = tmp_head->next_offset;
    count--;

    return reinterpret_cast<block_type *>(tmp_head);
  }

  result<> push(block_type &elem, offset_type &head, offset_type &count) {
    fail(count >= max_size(), "list full");
    fail(!owns(elem), "invalid pointer");

    auto *internal_elem = reinterpret_cast<internal_block_type *>(&elem);
    insert(*internal_elem, head, count);
    return {};
  }
};

template <std::size_t block_size, std::size_t block_count,
          typename unique_tag = void>
  requires nonzero_power_of_two<block_size, block_count>
class freelist {
public:
  using offset_type = smallest_t<block_count>;
  using block_type = std::array<std::byte, block_size>;
  static constexpr bool using_raw_ptr = std::is_void_v<unique_tag>;

private:
  static constexpr offset_type null_offset =
      std::numeric_limits<offset_type>::max();

  freelist_storage<block_size, block_count> _storage;
  inline static offset_type _head{null_offset};
  inline static offset_type _count{0};

public:
  std::byte *base() noexcept { return _storage.base(); }
  const std::byte *base() const noexcept { return _storage.base(); }

  constexpr size_t size() const noexcept { return _storage.size(); }
  constexpr size_t max_size() const noexcept { return _storage.max_size(); }
  bool is_full() const noexcept { return _storage.is_full(_count); }
  bool is_empty() const noexcept { return _storage.is_empty(_head); }

  bool owns(const block_type &elem) const noexcept {
    return _storage.owns(elem);
  }

  freelist() { reset(); }
  ~freelist() = default;

  void reset() { _storage.reset(_head, _count); }
  result<const block_type *> head() const { return _storage.head(_head); }
  result<block_type *> pop() { return _storage.pop(_head, _count); }
  result<> push(block_type &elem) { return _storage.push(elem, _head, _count); }
};
