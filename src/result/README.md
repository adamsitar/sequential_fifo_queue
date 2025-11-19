# Result & Logging Library

A modern C++20/23 library for ergonomic error handling and structured logging with zero-cost abstractions.

## Overview

This library provides two complementary components:

1. **Result Type** - Type-safe error handling based on `std::expected` with expressive macros
2. **Debug Logger** - Flexible, builder-pattern logging with compile-time elimination

Both components integrate seamlessly, using `std::source_location` for automatic location tracking and `std::format` for type-safe formatting.

## Features

### Error Handling

- 🎯 **Type-safe** - Built on `std::expected<T, error>`
- 🔄 **Propagation** - `ok()` macro for automatic error forwarding
- 💥 **Early returns** - `fail()` macro for conditional failures
- 🚨 **Fatal errors** - `fatal()` for unrecoverable conditions
- 📍 **Location tracking** - Automatic file/line/function capture
- 🔍 **Stack traces** - Optional backtrace on failures
- 📝 **Contextual logging** - Rich error messages with builder pattern

### Logging

- ⚡ **Zero overhead** - Compile-time elimination with `-DDISABLE_DEBUG_LOG`
- 🎨 **Colored output** - Terminal color support for readability
- 🔧 **Builder pattern** - Chainable configuration (`.location().func().stacktrace()`)
- 🔍 **Auto-stringify** - Variables logged with names via macro magic
- 📏 **Single-line output** - Industry-standard format for log aggregation
- 🎚️ **Flexible display** - Control path depth, function visibility, stack depth
- 🔀 **Format strings** - Full `std::format` support for messages

## Requirements

- **C++23** for `std::expected`, `std::source_location`, `std::format`, `std::print`
- **GCC 13+** or **Clang 16+** (tested with GCC 15.2.1)
- **Linux/Unix** (uses ANSI color codes)

## Quick Start

### Error Handling

```cpp
#include <result/result.h>

// Define your error codes
enum class error {
    generic,
    not_found,
    invalid_input,
    timeout
};

// Return results instead of exceptions
result<int> divide(int a, int b) {
    fail(b == 0, "division by zero");  // Early return on failure
    return a / b;
}

result<std::string> read_file(const char* path) {
    auto fd = open_file(path);
    fail(!fd, error::not_found);       // Can use error codes

    auto contents = ok(read_contents(fd));  // Propagate errors
    return contents;
}

// Fatal errors for unrecoverable conditions
void critical_operation(int value) {
    fatal(value < 0, "negative values not allowed");
    // Prints error with location + stacktrace, then calls std::abort()
}
```

### Debug Logging

```cpp
#include <result/log.h>

void example() {
    int x = 42, y = 100;

    // Variable logging with auto-stringification
    print(x, y);
    // Output: [Debug] example.cpp:5 x: 42 | y: 100

    // Formatted messages
    debug("Processing {} items", count);
    // Output: [Debug] example.cpp:8 "Processing 10 items"

    // Add function context
    print(value).func();
    // Output: [Debug] example.cpp:11 in void example() value: 42

    // Show more path context (0=filename, 1=parent/file, 2=grandparent/parent/file)
    debug("checkpoint").location(1);
    // Output: [Debug] src/example.cpp:14 "checkpoint"

    // Add context variables and stack trace
    debug("error occurred").location().ctx(VAR(x), VAR(y)).stacktrace(10);
    // Output: [Debug] example.cpp:17 in void example() "error occurred"
    // x: 42
    // y: 100
    // Stack trace:
    // ... (10 frames)
}
```

## API Reference

### Result Type

```cpp
template<typename T = void>
struct result : public std::expected<T, error>;
```

**Macros:**

| Macro | Purpose | Example |
|-------|---------|---------|
| `ok(expr)` | Propagate errors, unwrap success | `auto val = ok(try_parse(str));` |
| `unwrap(expr)` | Assert success, fatal on error | `auto val = unwrap(get_config());` |
| `fail(cond)` | Conditional early return | `fail(x < 0);` |
| `fail(cond, msg)` | Early return with message | `fail(fd == -1, "open failed");` |
| `fail(cond, error)` | Early return with error code | `fail(!valid, error::invalid_input);` |
| `fatal(cond)` | Fatal assertion | `fatal(ptr == nullptr);` |
| `fatal(cond, msg)` | Fatal with message | `fatal(size > MAX, "overflow");` |

**Fail Builder (chainable):**

```cpp
fail(condition, "message")
    .err(error::timeout)           // Set error code
    .ctx(VAR(x), VAR(y))          // Add context variables
    .stacktrace()                  // Include stack trace
    .silent();                     // Suppress logging
```

### Debug Logger

**Macros:**

| Macro | Purpose | Output Format |
|-------|---------|---------------|
| `print(vars...)` | Log variables | `var1: val1 \| var2: val2` |
| `debug(fmt, args...)` | Log formatted message | `"message text"` |

**Builder Methods:**

```cpp
debug("message")
    .location(depth)      // Show file:line (depth: 0=file, 1=dir/file, ...)
    .func()              // Add function name
    .ctx(VAR(x), VAR(y)) // Add context variables (printed separately)
    .stacktrace(max, skip) // Add stack trace (max depth, frames to skip)
```

**Compile-time Control:**

```bash
# Enable debug logging (default)
g++ -std=c++23 main.cpp

# Disable all debug logging (zero overhead)
g++ -std=c++23 -DDISABLE_DEBUG_LOG main.cpp
```

## Design Patterns

### 1. Error Propagation

The `ok()` macro implements Railway-Oriented Programming:

```cpp
result<User> authenticate(const string& token) {
    auto claims = ok(parse_jwt(token));     // Returns error if parse fails
    auto user_id = ok(extract_id(claims));  // Returns error if extraction fails
    auto user = ok(db.find_user(user_id));  // Returns error if not found
    return user;                             // Returns success
}
```

### 2. Builder Pattern Logging

RAII-based logging with method chaining:

```cpp
// Builder configures, destructor executes
debug("operation failed")
    .location()
    .ctx(VAR(attempt), VAR(reason))
    .stacktrace(5);
// All logging happens when the temporary destructs at the semicolon
```

### 3. Macro Metaprogramming

Uses C++20 `__VA_OPT__` for recursive expansion:

```cpp
print(x, y, z)
// Expands to:
dbglog::logger(..., dbglog::vars, "x", x, "y", y, "z", z)
// Variables automatically stringified and paired with values
```

## Advanced Usage

### Custom Error Types

```cpp
enum class network_error {
    connection_refused,
    timeout,
    dns_failure
};

// Specialize to_string() for your error type
const char* to_string(network_error e);
```

### Conditional Logging

```cpp
#ifdef VERBOSE_DEBUG
    print(state, flags, counter).location().func();
#else
    print(state);
#endif
```

### Integration with Existing Code

```cpp
// Wrap legacy error-code APIs
result<File> open_file(const char* path) {
    int fd = ::open(path, O_RDONLY);
    fail(fd < 0, error::not_found);
    return File{fd};
}

// Convert to/from std::expected
std::expected<int, string> legacy_function();

result<int> modern_wrapper() {
    auto res = legacy_function();
    fail(!res.has_value(), error::generic);
    return res.value();
}
```

## Performance Characteristics

- **Result Type**: Zero-overhead abstraction over `std::expected`
- **ok() macro**: Inlined, identical to manual error checking
- **fail() macro**: RAII builder, no allocations in success path
- **Debug logging**: Completely eliminated when compiled with `-DDISABLE_DEBUG_LOG`
- **Stack traces**: On-demand, only computed when explicitly requested

## Color Output

The logger uses ANSI color codes:

- **Gray** - File paths, function names, metadata
- **Yellow** - `[Fail]` headers
- **Red** - `[Fatal]` headers
- **Dim** - `[Debug]` headers

Colors automatically detected and disabled on non-TTY output.

## Thread Safety

⚠️ **Note**: This library is not thread-safe by design. For concurrent logging:

- Use per-thread logging
- Add external synchronization
- Consider async logging wrappers

## Future Enhancements

- [ ] Log levels (TRACE, DEBUG, INFO, WARN, ERROR)
- [ ] Multiple sinks (file, syslog, network)
- [ ] Structured logging (JSON output)
- [ ] Async logging support
- [ ] Custom formatters
- [ ] Per-module log filtering
