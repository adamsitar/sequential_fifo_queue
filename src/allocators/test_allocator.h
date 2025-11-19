#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <result/result.h>

// ============================================================================
// Simple Test Allocator
// ============================================================================
// A minimal allocator implementation for unit testing containers in isolation.
// Uses global new/delete to avoid coupling tests to complex custom allocator
// implementations.
// Implements the full allocator interface expected by containers (block
// allocation, homogeneous storage, standard pointers).
// ============================================================================

class simple_test_allocator : public std::pmr::memory_resource {
public:
  template <typename T> struct pointer_wrapper {
    T *ptr;

    pointer_wrapper() : ptr(nullptr) {}
    pointer_wrapper(T *p) : ptr(p) {}
    pointer_wrapper(void *p) : ptr(static_cast<T *>(p)) {}
    pointer_wrapper(std::nullptr_t) : ptr(nullptr) {}

    template <typename U>
    pointer_wrapper(pointer_wrapper<U> other)
        : ptr(reinterpret_cast<T *>(other.ptr)) {}

    T *operator->() const { return ptr; }
    T &operator*() const { return *ptr; }
    operator T *() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
    template <typename U> using rebind = pointer_wrapper<U>;
  };

  using pointer_type = pointer_wrapper<std::byte>;
  using block_type = std::byte *;

  static constexpr size_t block_size = 64;
  static constexpr size_t block_align = alignof(std::max_align_t);
  static constexpr size_t max_block_count = 1024;
  static constexpr size_t total_size = block_size * max_block_count;

  struct unique_tag {};
  using offset_type = std::ptrdiff_t;

  result<pointer_type> allocate_block() noexcept {
    try {
      void *mem = ::operator new(block_size, std::align_val_t(block_align));
      return pointer_type(mem);
    } catch (...) { return error::out_of_memory; }
  }

  result<> deallocate_block(pointer_type ptr) noexcept {
    if (ptr) {
      ::operator delete(static_cast<void *>(ptr.ptr),
                        std::align_val_t(block_align));
    }
    return {};
  }

  void reset() noexcept {}
  std::size_t size() const noexcept { return 0; }
  std::byte *base() const noexcept { return nullptr; }

  simple_test_allocator() = default;
  simple_test_allocator(simple_test_allocator const &) = default;
  simple_test_allocator &operator=(simple_test_allocator const &) = default;
  ~simple_test_allocator() override = default;

private:
  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    return ::operator new(bytes, std::align_val_t(alignment));
  }

  void do_deallocate(void *ptr, std::size_t bytes,
                     std::size_t alignment) override {
    ::operator delete(ptr, std::align_val_t(alignment));
  }

  bool
  do_is_equal(std::pmr::memory_resource const &other) const noexcept override {
    return this == &other;
  }
};
