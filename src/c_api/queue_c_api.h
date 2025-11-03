/**
 * @file queue_c_api.h
 * @brief C API for byte queue (FIFO)
 *
 * This is a pure C interface that hides the C++ template implementation.
 * Uses PIMPL pattern with opaque pointer to hide all C++ details.
 *
 * Thread Safety: Not thread-safe. Caller must synchronize access.
 * Memory Management: User must call destroy_queue() for each create_queue().
 */

#ifndef QUEUE_C_API_H
#define QUEUE_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Opaque handle to a byte queue.
 *
 * This is an incomplete type - users cannot see the implementation.
 * All operations go through the API functions below.
 */
typedef struct queue_handle queue_handle;

/**
 * @brief Create a new FIFO byte queue.
 *
 * Allocates and initializes a new queue instance. The queue dynamically
 * grows as needed when elements are pushed.
 *
 * @return Pointer to new queue handle, or NULL on allocation failure.
 *
 * @note Caller must call destroy_queue() when done to avoid memory leak.
 *
 * Example:
 * @code
 *   queue_handle *q = create_queue();
 *   if (!q) { handle_error(); }
 *   // ... use queue ...
 *   destroy_queue(q);
 * @endcode
 */
queue_handle *create_queue(void);

/**
 * @brief Destroy a queue and free all associated memory.
 *
 * Deallocates all internal structures and the queue itself.
 * After this call, the handle is invalid and must not be used.
 *
 * @param q Queue handle to destroy (obtained from create_queue)
 *
 * @note Passing NULL is safe (no-op).
 * @note Queue does not need to be empty before destruction.
 */
void destroy_queue(queue_handle *q);

/**
 * @brief Add a byte to the back of the queue.
 *
 * Pushes a byte to the end of the queue (FIFO order).
 * May trigger internal allocation if current storage is full.
 *
 * @param q Queue handle
 * @param byte Byte value to enqueue (0-255)
 * @return true on success, false on allocation failure
 *
 * @note Passing NULL for q results in undefined behavior.
 *
 * Example:
 * @code
 *   if (!enqueue_byte(q, 42)) {
 *     fprintf(stderr, "Failed to enqueue\n");
 *   }
 * @endcode
 */
bool enqueue_byte(queue_handle *q, uint8_t byte);

/**
 * @brief Remove and return the next byte from the front of the queue.
 *
 * Removes the oldest byte from the queue (FIFO order).
 * May trigger internal deallocation if storage becomes empty.
 *
 * @param q Queue handle
 * @param[out] out_byte Pointer to store the dequeued byte
 * @return true if byte was dequeued, false if queue was empty
 *
 * @note Passing NULL for q or out_byte results in undefined behavior.
 *
 * Example:
 * @code
 *   uint8_t byte;
 *   if (dequeue_byte(q, &byte)) {
 *     printf("Got byte: %u\n", byte);
 *   } else {
 *     printf("Queue is empty\n");
 *   }
 * @endcode
 */
bool dequeue_byte(queue_handle *q, uint8_t *out_byte);

/**
 * @brief Check if the queue is empty.
 *
 * @param q Queue handle
 * @return true if queue contains no elements, false otherwise
 *
 * @note Passing NULL for q results in undefined behavior.
 */
bool queue_is_empty(const queue_handle *q);

/**
 * @brief Get the number of bytes currently in the queue.
 *
 * @param q Queue handle
 * @return Number of bytes in queue (0 if empty)
 *
 * @note Passing NULL for q results in undefined behavior.
 * @note This is an O(n) operation where n = number of internal buffers.
 */
size_t queue_size(const queue_handle *q);

/**
 * @brief Remove all bytes from the queue.
 *
 * Empties the queue and deallocates all internal storage.
 * Queue remains valid and can continue to be used after clearing.
 *
 * @param q Queue handle
 *
 * @note Passing NULL for q results in undefined behavior.
 */
void queue_clear(queue_handle *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_C_API_H */
