# Sequential FIFO Queue

This project implements a queue datastructure specialized for extremely memory-constrained environments. The design prioritizes minimal memory overhead while maintaining standard queue semantics and RAII principles.

## Architecture Overview

The queue is built as a linked list of ring buffers, combining the dynamic growth of linked lists with the cache-friendly locality of fixed-size circular buffers. This hybrid approach provides amortized O(1) operations while minimizing metadata overhead.

### Core Components

**Queue**
The top-level datastructure that presents a standard FIFO interface. Internally maintains an offset_list where each node contains a ring_buffer. As elements are pushed, new ring_buffers are allocated when the current one fills. As elements are popped and ring_buffers empty, they are automatically deallocated.

**Ring Buffer**
A fixed-capacity circular buffer that stores the actual queue elements. Uses a thin storage pointer to reference its backing memory, consuming only 1-2 bytes instead of the typical 8-byte pointer. The buffer tracks head, tail, and free space using the smallest integer type that can represent its capacity.

**Offset List**
A singly-linked list implemented using segmented pointers for node linking. Maintains both head and tail pointers for O(1) access to both ends. Each node contains a ring_buffer and uses segmented addressing to reference the next node.

**Segmented Pointer**
A two-level addressing scheme that replaces traditional 8-byte pointers with 1-2 byte offsets. Consists of a segment ID and an offset within that segment. The segment ID references one of the memory blocks managed by the allocator, while the offset locates the specific address within that block.

### Memory Allocators

**Local Buffer**
The top-level allocator that manages a fixed pool of uniformly-sized memory blocks. All memory in the system is allocated from this single contiguous buffer. Block size and count are configured at compile time.

**Dynamic Buffer**
A mid-level allocator that receives large blocks from the local buffer and subdivides them into smaller blocks on demand. Manages its own metadata within the allocated blocks, avoiding external storage overhead. Multiple dynamic buffers can share a single local buffer.

### Static Allocator Pattern

All datastructures use static allocator pointers rather than per-instance pointers. Since each queue type is templated on its allocator types, all instances of a given queue configuration naturally share the same allocators. This pattern eliminates 24 bytes of allocator pointer overhead per queue instance.

A typical queue instance consists of just the offset_list's head and tail segmented pointers, totaling approximately 4 bytes. This represents a 85% reduction compared to a traditional implementation with 8-byte pointers and per-instance allocator references.

## Memory Layout

All metadata is stored within allocated blocks. There is no external bookkeeping data. The local buffer contains:

- Ring buffer storage blocks (raw element storage)
- Offset list node blocks (node metadata + ring_buffer control structures)
- Dynamic buffer metadata blocks (segment tracking and free lists)

The entire queue system operates within a fixed memory budget determined by the local buffer configuration. No heap allocations occur outside this controlled pool.

## Error Handling

All allocation operations return custom result types rather than throwing exceptions, making the code suitable for freestanding and embedded environments where exception support may be unavailable.
The result type is a wrapper for std::expected, but with additional feature for more ergonomics.

## Building

A CMake Preset for linux is provided. Project has been tested only with the debug configuration with the Clang (version 21) compiler.
In order to build the tests:

```
cmake --preset=linux_debug
cmake --build --preset=linux_debug --target allocators_test
cmake --build --preset=linux_debug --target datastructures_test
```

Then to run:

```
./build/linux_debug/src/allocators/allocators_test
./build/linux_debug/src/datastructures/datastructures_test
```

The libraries themselves are distributed as header only INTERFACE targets.
The cmake configuration includes a directive to generate a compile database, which works with clangd LSP server.
