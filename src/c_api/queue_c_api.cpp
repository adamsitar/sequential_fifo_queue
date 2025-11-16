#include "queue_c_api.h"

#include <cstddef>
#include <growing_pool.h>
#include <local_buffer.h>
#include <queue.h>

using local_allocator_type = local_buffer(16, 128);
using list_allocator_type = growing_pool(8, 32, local_allocator_type);
using queue_allocator_type = growing_pool(8, 32, local_allocator_type);
using byte_queue = queue<std::byte, 16, local_allocator_type, list_allocator_type>;

struct queue_handle {
  byte_queue impl;
  queue_allocator_type::pointer_type self_allocation; // Track for deallocation

  explicit queue_handle(local_allocator_type *local_alloc,
                        list_allocator_type *list_alloc,
                        queue_allocator_type::pointer_type alloc_ptr)
      : impl(local_alloc, list_alloc), self_allocation(alloc_ptr) {}
};

extern "C" {

__attribute__((weak)) void on_out_of_memory(void) { abort(); }
__attribute__((weak)) void on_illegal_operation(void) { abort(); }

Q *create_queue(local_allocator_t local_alloc,
                list_allocator_t list_alloc,
                queue_allocator_t queue_alloc) {
  auto *local_allocator_ptr = static_cast<local_allocator_type*>(local_alloc);
  auto *list_allocator_ptr = static_cast<list_allocator_type*>(list_alloc);
  auto *queue_allocator_ptr = static_cast<queue_allocator_type*>(queue_alloc);

  auto block_result = queue_allocator_ptr->allocate_block();
  if (!block_result) {
    on_out_of_memory();
    __builtin_unreachable();
  }

  auto ptr = block_result.value();
  void *block_ptr = ptr;
  auto *q = new (block_ptr) queue_handle(local_allocator_ptr,
                                          list_allocator_ptr,
                                          *block_result);

  return q;
}

void destroy_queue(Q *q, queue_allocator_t queue_alloc) {
  if (q == nullptr) {
    on_illegal_operation();
    __builtin_unreachable();
  }

  auto *queue_allocator_ptr = static_cast<queue_allocator_type*>(queue_alloc);
  auto alloc_ptr = q->self_allocation;
  q->~queue_handle();
  queue_allocator_ptr->deallocate_block(alloc_ptr);
}

void enqueue_byte(Q *q, unsigned char b) {
  if (q == nullptr) {
    on_illegal_operation();
    __builtin_unreachable();
  }

  auto result = q->impl.push(std::byte{b});
  if (!result) {
    on_out_of_memory();
    __builtin_unreachable();
  }
}

unsigned char dequeue_byte(Q *q) {
  if (q == nullptr) {
    on_illegal_operation();
    __builtin_unreachable();
  }

  auto result = q->impl.pop();
  if (!result) {
    on_illegal_operation();
    __builtin_unreachable();
  }

  return static_cast<unsigned char>(*result);
}

bool queue_is_empty(const queue_handle *q) {
  if (q == nullptr) { return true; }

  return q->impl.empty();
}

size_t queue_size(const queue_handle *q) {
  if (q == nullptr) { return 0; }

  return q->impl.size();
}

void queue_clear(queue_handle *q) {
  if (q != nullptr) { q->impl.clear(); }
}

} // extern "C"
