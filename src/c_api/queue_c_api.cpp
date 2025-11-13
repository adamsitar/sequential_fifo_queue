#include "queue_c_api.h"

#include <cstddef>
#include <growing_pool.h>
#include <local_buffer.h>
#include <queue.h>

using local_allocator = local_buffer(16, 128);
using list_allocator = growing_pool(8, 32, local_allocator);
using queue_allocator = growing_pool(8, 32, local_allocator);
using byte_queue = queue<std::byte, 16, local_allocator, list_allocator>;

static local_allocator g_local_allocator;
static list_allocator g_list_allocator(&g_local_allocator);
static queue_allocator g_queue_allocator(&g_local_allocator);

struct queue_handle {
  byte_queue impl;
  queue_allocator::pointer_type self_allocation; // Track for deallocation

  explicit queue_handle(queue_allocator::pointer_type alloc_ptr)
      : impl(&g_local_allocator, &g_list_allocator),
        self_allocation(alloc_ptr) {}
};

extern "C" {

__attribute__((weak)) void on_out_of_memory(void) { abort(); }
__attribute__((weak)) void on_illegal_operation(void) { abort(); }

Q *create_queue(void) {
  auto block_result = g_queue_allocator.allocate_block();
  if (!block_result) {
    on_out_of_memory();
    __builtin_unreachable();
  }

  void *block_ptr = static_cast<void *>(*block_result);
  auto *q = new (block_ptr) queue_handle(*block_result);

  return q;
}

void destroy_queue(Q *q) {
  if (q == nullptr) {
    on_illegal_operation();
    __builtin_unreachable();
  }

  auto alloc_ptr = q->self_allocation;
  q->~queue_handle();
  g_queue_allocator.deallocate_block(alloc_ptr);
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
