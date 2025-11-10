# Modern C++23 Iterator Library

A lightweight, header-only iterator library leveraging C++23's **deducing this** feature to provide clean, efficient iterator and container interfaces.

## Overview

This library provides two main components:

1. **Iterator Facades** (`iterator_facade.h`) - Build custom iterators from minimal primitives
2. **Container Interfaces** (`container_interface.h`) - Eliminate container boilerplate

Inspired by Boost.Iterator but modernized with C++23 features for superior ergonomics and zero overhead.

---

## Iterator Facades

### Hierarchy

```
input_iterator_facade / output_iterator_facade
        ↓
forward_iterator_facade
        ↓
bidirectional_iterator_facade
        ↓
random_access_iterator_facade
        ↓
contiguous_iterator_facade
```

### 1. Input Iterator Facade

**Use for:** Single-pass read-only iteration (e.g., input streams)

**Primitives required:**

```cpp
reference dereference() const;
void increment();
bool equals(const iterator& other) const;
```

**Provides:** `*`, `->`, `++`, `++(int)`, `==`, typedefs

**Example:**

```cpp
class stream_iterator : public input_iterator_facade<int> {
    std::istream* _stream;
    int _value;

public:
    explicit stream_iterator(std::istream& s) : _stream(&s) {
        *_stream >> _value;
    }

    int dereference() const { return _value; }
    void increment() { *_stream >> _value; }
    bool equals(const stream_iterator& o) const {
        return _stream == o._stream;
    }
};
```

---

### 2. Output Iterator Facade

**Use for:** Single-pass write-only iteration (e.g., output streams)

**Primitives required:**

```cpp
auto dereference();  // Returns proxy or reference
void increment();
```

**Provides:** `*`, `++`, `++(int)`, typedefs

**Example:**

```cpp
class ostream_iterator : public output_iterator_facade<int> {
    std::ostream* _stream;

public:
    explicit ostream_iterator(std::ostream& s) : _stream(&s) {}

    auto dereference() {
        return [this](int val) { *_stream << val; };
    }
    void increment() { /* no-op for output */ }
};
```

---

### 3. Forward Iterator Facade

**Use for:** Multi-pass read/write iteration (e.g., singly-linked lists)

**Primitives required:**

```cpp
reference dereference() const;
void increment();
bool equals(const iterator& other) const;
```

**Provides:** `*`, `->`, `++`, `++(int)`, `==`, typedefs

**Example:**

```cpp
template <typename T>
class slist_iterator : public forward_iterator_facade<T> {
    node<T>* _current;

public:
    explicit slist_iterator(node<T>* n) : _current(n) {}

    T& dereference() const { return _current->value; }
    void increment() { _current = _current->next; }
    bool equals(const slist_iterator& o) const {
        return _current == o._current;
    }
};
```

---

### 4. Bidirectional Iterator Facade

**Use for:** Forward iteration + backward traversal (e.g., doubly-linked lists)

**Adds to forward:**

```cpp
void decrement();
```

**Provides:** `--`, `--(int)` (plus all forward operations)

**Example:**

```cpp
template <typename T>
class dlist_iterator : public bidirectional_iterator_facade<T> {
    node<T>* _current;

public:
    // ... forward primitives ...

    void decrement() { _current = _current->prev; }
};
```

---

### 5. Random Access Iterator Facade

**Use for:** Constant-time jumps (e.g., arrays, deques, ring buffers)

**Adds to bidirectional:**

```cpp
void advance(difference_type n);
difference_type distance_to(const iterator& other) const;
```

**Provides:** `+`, `-`, `+=`, `-=`, `[]`, `<=>` (plus all bidirectional operations)

**Example:**

```cpp
template <typename T, size_t N>
class ring_buffer_iterator : public random_access_iterator_facade<T> {
    T* _buffer;
    size_t _pos, _capacity;

public:
    T& dereference() const { return _buffer[_pos]; }

    void advance(difference_type n) {
        _pos = (_pos + n + _capacity) % _capacity;
    }

    difference_type distance_to(const ring_buffer_iterator& o) const {
        return static_cast<difference_type>(o._pos - _pos);
    }

    bool equals(const ring_buffer_iterator& o) const {
        return _pos == o._pos;
    }
};
```

---

### 6. Contiguous Iterator Facade (C++20)

**Use for:** Iterators over contiguous memory with `std::to_address` support

**Adds to random access:**

```cpp
Value* to_address() const;
```

**Provides:** Contiguous memory guarantees for standard algorithms

**Example:**

```cpp
template <typename T>
class array_iterator : public contiguous_iterator_facade<T> {
    T* _ptr;

public:
    explicit array_iterator(T* p) : _ptr(p) {}

    T& dereference() const { return *_ptr; }
    void advance(difference_type n) { _ptr += n; }
    difference_type distance_to(const array_iterator& o) const {
        return o._ptr - _ptr;
    }
    bool equals(const array_iterator& o) const { return _ptr == o._ptr; }
    T* to_address() const { return _ptr; }
};
```

---

## Container Interfaces

Eliminate boilerplate for container iterator methods.

### 1. Forward Container Interface

**Provides:** `cbegin()`, `cend()`
**Requires:** Container implements `begin()`, `end()`

```cpp
template <typename T, typename Alloc>
class singly_linked_list
    : public forward_container_iterator_interface<singly_linked_list<T, Alloc>> {

    class iterator : public forward_iterator_facade<T> { /* ... */ };

    iterator begin();
    iterator end();
    // cbegin/cend provided automatically
};
```

---

### 2. Bidirectional Container Interface

**Provides:** `cbegin()`, `cend()`, `rbegin()`, `rend()`, `crbegin()`, `crend()`
**Requires:** Container implements `begin()`, `end()`

```cpp
template <typename T, typename Alloc>
class vector
    : public bidirectional_container_iterator_interface<vector<T, Alloc>> {

    class iterator : public random_access_iterator_facade<T> { /* ... */ };

    iterator begin();
    iterator end();
    // All 6 methods provided automatically
};
```

---

### 3. Before-Begin Iterator Mixin

**Provides:** `cbefore_begin()`
**Requires:** Container implements `before_begin()`

```cpp
template <typename T, typename Alloc>
class forward_list
    : public forward_container_iterator_interface<forward_list<T, Alloc>>,
      public before_begin_iterator_interface<forward_list<T, Alloc>> {

    iterator before_begin();
    iterator begin();
    iterator end();
    // cbeforebegin/cbegin/cend provided
};
```

---

## Key Advantages Over Boost.Iterator

| Feature | Boost.Iterator | This Library |
|---------|----------------|--------------|
| **CRTP Syntax** | Explicit `derived()` casts | Deducing this (automatic) |
| **Template Parameters** | 5+ parameters | 1-3 parameters |
| **Boilerplate** | ~100 lines/iterator | ~10 lines/iterator |
| **Container Interface** | Not provided | Included |
| **Standard Compliance** | C++03/11 | C++23 |
| **Dependencies** | Boost ecosystem | Header-only, standalone |

---

## Integration

### CMake

```cmake
target_include_directories(your_target PRIVATE ${PROJECT_SOURCE_DIR}/src/iterators)
```

### Usage

```cpp
#include <iterator_facade.h>
#include <container_interface.h>
```

---

## Future Enhancements

Potential additions:

- **Iterator adaptors:** `filter_iterator`, `transform_iterator`, `zip_iterator`
- **Range adaptors:** Integration with C++20 ranges
- **Concepts:** Formal concept definitions for facades
