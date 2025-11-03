/**
 * @file queue_c_api.cpp
 * @brief C++ implementation of C API for byte queue
 *
 * PIMPL pattern: Hides all C++ template complexity behind opaque pointer.
 * The C header sees only 'struct queue_handle', while this file contains
 * the actual template instantiation.
 */

#include "queue_c_api.h"

#include <cstddef>
#include <dynamic_buffer.h>
#include <local_buffer.h>
#include <queue.h>

// ============================================================================
// Allocator Configuration
// ============================================================================

// Top-level allocator: Fixed 32KB buffer for all memory (embedded/freestanding)
// Block size: 2048 bytes
// Block count: 16
// Total: 32KB managed entirely within this buffer
using local_allocator = local_buffer(16, 128);

// List node allocator: Small blocks for offset_list nodes
// Block size: 16 bytes (enough for ring_buffer_node: ~8-12 bytes)
// Source: Gets 2048-byte blocks from local_allocator, divides into 128×16-byte
// blocks
using list_allocator = dynamic_buffer(8, local_allocator);

// Queue instance allocator: Larger blocks for queue instances
// Block size: 64 bytes (enough for queue instance: ~48 bytes)
// Source: Gets 2048-byte blocks from local_allocator, divides into 32×64-byte
// blocks
using queue_allocator = dynamic_buffer(16, local_allocator);

// Queue configuration
// Element type: std::byte (for byte queue)
// Ring buffer capacity: 64 bytes per buffer
// Allocators: Use shared allocators
using byte_queue = queue<std::byte, 16, local_allocator, list_allocator>;

// ============================================================================
// Global Shared Allocators
// ============================================================================

/**
 * @brief Global allocators shared across all queue instances.
 *
 * Embedded/Freestanding Environment:
 * - All memory managed in a fixed 32KB buffer (no heap!)
 * - Three-level allocation hierarchy:
 *   1. local_allocator: Top-level 2048-byte blocks
 *   2. list_allocator: Small 16-byte blocks for list nodes
 *   3. queue_allocator: Medium 64-byte blocks for queue instances
 *
 * Benefits:
 * - No malloc/free dependency (freestanding compatible)
 * - Amortizes metadata overhead across all queues
 * - Supports 64+ queue instances efficiently
 *
 * Thread Safety: Not thread-safe. Caller must synchronize if using from
 * multiple threads.
 */
static local_allocator g_local_allocator;
static list_allocator g_list_allocator(&g_local_allocator);
static queue_allocator g_queue_allocator(&g_local_allocator);

// ============================================================================
// PIMPL Implementation
// ============================================================================

/**
 * @brief Internal queue handle structure (PIMPL).
 *
 * This is what 'queue_handle' actually points to. The C header only sees
 * an incomplete type, so all template complexity is hidden.
 *
 * Memory Management:
 * - Instance is placement new'd into a block from queue_allocator
 * - self_allocation tracks the block for later deallocation
 * - No heap allocation (malloc/new) used - fully embedded compatible
 */
struct queue_handle {
  byte_queue impl;
  queue_allocator::pointer_type self_allocation; // Track for deallocation

  explicit queue_handle(queue_allocator::pointer_type alloc_ptr)
      : impl(&g_local_allocator, &g_list_allocator),
        self_allocation(alloc_ptr) {}
};

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

queue_handle *create_queue(void) {
  // Allocate block from queue_allocator (no heap allocation!)
  auto block_result = g_queue_allocator.allocate_block();
  if (!block_result) {
    return nullptr; // Out of memory
  }

  auto block_ptr = *block_result;

  // Convert to raw pointer for placement new
  void *raw_mem = static_cast<void *>(block_ptr);

  // Placement new the queue_handle into the allocated block
  queue_handle *q = new (raw_mem) queue_handle(block_ptr);

  return q;
}

void destroy_queue(queue_handle *q) {
  if (q == nullptr) {
    return;
  }

  // Extract allocation pointer before destruction
  auto alloc_ptr = q->self_allocation;

  // Call destructor (destroys queue, which clears all ring_buffers)
  q->~queue_handle();

  // Deallocate the block (no heap free!)
  g_queue_allocator.deallocate_block(alloc_ptr);
}

bool enqueue_byte(queue_handle *q, uint8_t byte) {
  if (q == nullptr) {
    return false;
  }

  auto result = q->impl.push(std::byte{byte});
  return result.has_value();
}

bool dequeue_byte(queue_handle *q, uint8_t *out_byte) {
  if (q == nullptr || out_byte == nullptr) {
    return false;
  }

  auto result = q->impl.pop();
  if (!result) {
    return false;
  }

  *out_byte = static_cast<uint8_t>(*result);
  return true;
}

bool queue_is_empty(const queue_handle *q) {
  if (q == nullptr) {
    return true; // Treat null as empty
  }

  return q->impl.empty();
}

size_t queue_size(const queue_handle *q) {
  if (q == nullptr) {
    return 0;
  }

  return q->impl.size();
}

void queue_clear(queue_handle *q) {
  if (q != nullptr) {
    q->impl.clear();
  }
}

} // extern "C"
