#ifndef QUEUE_C_API_H
#define QUEUE_C_API_H

#include <cstdlib>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Failure callbacks - default implementations call abort()
// Tests can override these to catch and verify failure conditions
void on_out_of_memory(void);
void on_illegal_operation(void);

typedef struct queue_handle queue_handle;
typedef queue_handle Q; // Alias for assignment compatibility

// Opaque allocator types for C interface
// (should be homogenous_allocator_base* in C++)
typedef void* allocator_t;

Q *create_queue(allocator_t local_alloc, allocator_t list_alloc, allocator_t queue_alloc);
void destroy_queue(Q *q, allocator_t queue_alloc);
void enqueue_byte(Q *q, unsigned char b);
unsigned char dequeue_byte(Q *q);

// Additional utility functions
bool queue_is_empty(const queue_handle *q);
size_t queue_size(const queue_handle *q);
void queue_clear(queue_handle *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_C_API_H */
