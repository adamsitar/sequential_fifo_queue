# Sequential FIFO Queue

> ⚠️This readme may not be entirely in sync with the state of the project, as the project is WIP

This project implements a queue datastructure specialized for extremely memory-constrained environments.

## Structure

The project consists these components:

- **`src/allocators/`** - Memory allocators that implement optimizations via usage of custom pointers.
- **`src/datastructures/`** - Datastructures that support the usage of custom pointers.
- **`src/result/`** - Error handling + logging library.
- **`src/iterators/`** - Iterator facade library, essentially helper templates to reduce iterator boilerplate (+ generic test suite).
- **`src/core/`** - Core types, utilities.
- **`src/c_api/`** - Deprecated C pimpl pattern.
- **`src/pch/`** - Precompiled header support, may or may not work.

---

## Overview

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
Manages a fixed number of memory segments, each subdivided into uniform blocks. Provides the foundation for growing_pool's scalability, is intended only for interal use by the `growing_pool`.

### Static Allocator Pattern

All datastructures use static allocator pointers rather than per-instance pointers. Since each queue type is templated on its allocator types, all instances of a given queue configuration naturally share the same allocators.
A typical queue instance consists of just the offset_list's head and tail segmented pointers, totaling approximately 4 bytes.

---

## Memory Layout

All metadata is stored within allocated blocks. The local buffer contains:

- Ring buffer storage blocks (raw element storage)
- Offset list node blocks (node metadata + ring_buffer control structures)
- Growing pool metadata blocks (segment tracking and free lists)

The entire queue system operates within a fixed memory budget determined by the local buffer configuration. No heap allocations occur outside this controlled pool.

---

## Building

A CMake Preset for Linux is provided. The project has been tested only with the debug configuration using the Clang (version 21) compiler.

### Basic Build

> ⚠️The `datastructures_test` suite contains the exact tests/cases realted to the assignment

```bash
cmake --preset=linux_debug
cmake --build --preset=linux_debug --target allocators_test
cmake --build --preset=linux_debug --target datastructures_test 
# deprecated for now
# cmake --build --preset=linux_debug --target c_api ```

### Build with Precompiled Headers 

```bash
cmake --preset=linux_debug -DITERATORS_USE_PCH=ON
cmake --build --preset=linux_debug
```

### Running Tests

```bash
# Datastructure tests
./build/linux_debug/src/datastructures/datastructures_test
# Allocator tests
./build/linux_debug/src/allocators/allocators_test

```

---

## Library Distribution

All libraries are distributed as header-only INTERFACE targets.
The CMake configuration includes a directive to generate a compile database, which works with clangd LSP server.

---

## Requirements

- **Compiler**: C++23 support required
  - GCC 13+ or Clang 16+ recommended
- **CMake**: 3.16+ (for precompiled headers support)
- **Platform**: Linux (tested)
