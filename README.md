# Sequential FIFO Queue

> ⚠️This readme may not be entirely in sync with the state of the project, as the project WIP

This project implements a queue datastructure specialized for extremely memory-constrained environments. The design prioritizes minimal memory overhead while maintaining standard queue semantics and RAII principles.

## Project Structure

The project consists of three main libraries:

- **`src/iterators/`** - Standalone C++23 iterator library (header-only)
- **`src/allocators/`** - Memory allocators for constrained environments
- **`src/datastructures/`** - Queue, ring buffer, and linked list implementations
- **`src/core/`** - Core types, pointer abstractions, and utilities

---

## Architecture Overview

The queue is built as a linked list of ring buffers, combining the dynamic growth of linked lists with the cache-friendly locality of fixed-size circular buffers. This hybrid approach provides amortized O(1) operations while minimizing metadata overhead.

### Core Components

**Queue**
The top-level datastructure that presents a standard FIFO interface. Internally maintains an offset_list where each node contains a ring_buffer. As elements are pushed, new ring_buffers are allocated when the current one fills. As elements are popped and ring_buffers empty, they are automatically deallocated.

**Ring Buffer**
A fixed-capacity circular buffer that stores the actual queue elements. Uses a thin storage pointer to reference its backing memory, consuming only 1-2 bytes instead of the typical 8-byte pointer. The buffer tracks head, tail, and free space using the smallest integer type that can represent its capacity.

**Offset List**
A singly-linked list implemented using segmented pointers for node linking. Maintains both head and tail pointers for O(1) access to both ends. Each node contains a ring_buffer and uses segmented addressing to reference the next node.

**Intrusive Singly-Linked List**
A header-only intrusive list implementation for managing pre-allocated nodes (e.g., allocator internal structures). Does not allocate memory itself - caller provides nodes. Works with custom pointer types (thin_ptr, segmented_ptr, etc.).

**Segmented Pointer**
A two-level addressing scheme that replaces traditional 8-byte pointers with 1-2 byte offsets. Consists of a segment ID and an offset within that segment. The segment ID references one of the memory blocks managed by the allocator, while the offset locates the specific address within that block.

### Memory Allocators

**Local Buffer**
The top-level allocator that manages a fixed pool of uniformly-sized memory blocks. All memory in the system is allocated from this single contiguous buffer. Block size and count are configured at compile time.

**Growing Pool**
Allocator that dynamically grows by allocating new segment managers on demand. Provides effectively unlimited capacity (within upstream limits) while maintaining compact pointer representations using growing_pool_ptr.

**Segment Manager**
Manages a fixed number of memory segments, each subdivided into uniform blocks. Provides the foundation for growing_pool's scalability, is only for interal use by the `growing_pool`.

### Static Allocator Pattern

All datastructures use static allocator pointers rather than per-instance pointers. Since each queue type is templated on its allocator types, all instances of a given queue configuration naturally share the same allocators. This pattern eliminates 24 bytes of allocator pointer overhead per queue instance.

A typical queue instance consists of just the offset_list's head and tail segmented pointers, totaling approximately 4 bytes. This represents a 85% reduction compared to a traditional implementation with 8-byte pointers and per-instance allocator references.

---

## Iterator Library

### Modern C++23 Iterator Facades

The project includes a standalone, header-only iterator library (`src/iterators/`) leveraging C++23's **deducing this** feature to provide clean, efficient iterator and container interfaces.

**Key Features:**

- **6 Iterator Facades**: input, output, forward, bidirectional, random-access, contiguous
- **3 Container Interfaces**: Eliminate boilerplate for begin/end/rbegin/rend methods
- **80-90% Code Reduction**: Build custom iterators from 3-4 primitives instead of 15+ operators
- **Zero Overhead**: Header-only, no runtime cost
- **Modern C++23**: Superior to Boost.Iterator with cleaner syntax

**Example:**

```cpp
// Before: ~50 lines of boilerplate
class my_iterator {
  // 5 typedefs + 15 operators...
};

// After: ~12 lines using facade
class my_iterator : public forward_iterator_facade<T> {
  T& dereference() const { return *_ptr; }
  void increment() { ++_ptr; }
  bool equals(const my_iterator& o) const { return _ptr == o._ptr; }
};
```

**Documentation:**

- Iterator Library: `src/iterators/README.md`

---

## Memory Layout

All metadata is stored within allocated blocks. The local buffer contains:

- Ring buffer storage blocks (raw element storage)
- Offset list node blocks (node metadata + ring_buffer control structures)
- Growing pool metadata blocks (segment tracking and free lists)

The entire queue system operates within a fixed memory budget determined by the local buffer configuration. No heap allocations occur outside this controlled pool.

---

## Error Handling

All allocation operations return custom result types rather than throwing exceptions, making the code suitable for freestanding and embedded environments where exception support may be unavailable. The result type is a wrapper for std::expected, with additional features for ergonomics.

---

## Building

A CMake Preset for Linux is provided. The project has been tested only with the debug configuration using the Clang (version 21) compiler.

### Basic Build

```bash
cmake --preset=linux_debug
cmake --build --preset=linux_debug --target allocators_test
cmake --build --preset=linux_debug --target datastructures_test
```

### Build with Precompiled Headers (4-6x Faster)

```bash
cmake --preset=linux_debug -DITERATORS_USE_PCH=ON
cmake --build --preset=linux_debug
```

### Running Tests

```bash
# Allocator tests
./build/linux_debug/src/allocators/allocators_test

# Datastructure tests
./build/linux_debug/src/datastructures/datastructures_test
```

---

## Library Distribution

All libraries are distributed as header-only INTERFACE targets:

- **`iterators`** - Standalone iterator library (C++23 only dependency)
- **`core`** - Core types and utilities
- **`allocators`** - Memory allocators
- **`datastructures`** - Queue, ring buffer, offset list

The CMake configuration includes a directive to generate a compile database, which works with clangd LSP server.

---

## Code Metrics

**Iterator Boilerplate Reduction:**

- offset_list: 50 lines → 12 lines
- ring_buffer: 85 lines → 15 lines
- intrusive_slist: 45 lines → 12 lines
- **Total**: ~265 lines eliminated across 4 containers

**Memory Overhead:**

- Traditional queue instance: ~32 bytes (8-byte pointers + allocator pointers)
- This implementation: ~4 bytes (segmented pointers only)
- **Reduction**: 85% smaller per-instance overhead

---

## Requirements

- **Compiler**: C++23 support required
  - GCC 13+ or Clang 16+ recommended
- **CMake**: 3.16+ (for precompiled headers support)
- **Platform**: Linux (tested)

---

## Design Philosophy

1. **Zero-cost abstractions**: Template-based design with no runtime overhead
2. **Memory efficiency**: Compact representations using custom pointer types
3. **Modern C++**: Leverage C++23 features (deducing this, concepts, std::expected)
4. **Embedded-friendly**: No exceptions, no heap allocations outside controlled pool
5. **Reusable components**: Iterator library designed as standalone, redistributable module
