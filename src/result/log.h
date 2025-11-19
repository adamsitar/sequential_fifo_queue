#pragma once
#include <functional>
#include <print>
#include <result/error.h>
#include <source_location>
#include <stacktrace>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dbglog {

namespace color {
constexpr const char *reset = "\033[0m";
constexpr const char *red = "\033[31m";
constexpr const char *green = "\033[32m";
constexpr const char *yellow = "\033[33m";
constexpr const char *blue = "\033[34m";
constexpr const char *magenta = "\033[35m";
constexpr const char *cyan = "\033[36m";
constexpr const char *white = "\033[37m";
constexpr const char *bold = "\033[1m";
constexpr const char *dim = "\033[2m";
} // namespace color

// Truncate path to show only the last N components
// depth=0: just filename, depth=1: parent/filename, etc.
inline std::string_view truncate_path(std::string_view path,
                                      size_t depth = 0) noexcept {
  if (depth == 0) {
    // Just return filename
    auto last_slash = path.rfind('/');
    if (last_slash == std::string_view::npos) { return path; }
    return path.substr(last_slash + 1);
  }

  // Find the Nth slash from the end
  size_t pos = path.size();
  for (size_t i = 0; i <= depth; ++i) {
    if (pos == 0) { return path; }
    pos = path.rfind('/', pos - 1);
    if (pos == std::string_view::npos) { return path; }
  }

  return path.substr(pos + 1);
}

inline void stacktrace(std::size_t skip_frames = 0,
                       std::size_t max_depth = 4) noexcept {
  std::println(stderr, "Stack trace:");
  std::print(stderr, "{}\n",
             std::to_string(std::stacktrace::current(skip_frames, max_depth)));
}

// Tag type to distinguish variable logging from message logging
struct vars_tag {};
inline constexpr vars_tag vars;

// Format a single name-value pair as a string
template <typename T>
inline std::string format_pair(std::string_view name, T &&value) {
  if constexpr (std::is_pointer_v<std::decay_t<T>>) {
    return std::format("{0}: {1}", name, static_cast<const void *>(value));
  } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
    return std::format("{0}: {1}", name, value ? "true" : "false");
  } else {
    return std::format("{0}: {1}", name, value);
  }
}

// Build a string of name-value pairs separated by " | "
inline std::string build_pairs_string() { return ""; }

template <typename First, typename Second, typename... Rest>
inline std::string build_pairs_string(First &&name, Second &&value,
                                      Rest &&...rest) {
  std::string result = format_pair(name, std::forward<Second>(value));

  if constexpr (sizeof...(Rest) > 0) {
    result += " | ";
    result += build_pairs_string(std::forward<Rest>(rest)...);
  }

  return result;
}

// Legacy function for printing pairs line-by-line (used by context)
inline void log_pairs() noexcept {}

template <typename First, typename Second, typename... Rest>
inline void log_pairs(First &&name, Second &&value, Rest &&...rest) noexcept {
  if constexpr (std::is_pointer_v<std::decay_t<Second>>) {
    std::println(stderr, "{0}: {1}", name, static_cast<const void *>(value));
  } else if constexpr (std::is_same_v<std::decay_t<Second>, bool>) {
    std::println(stderr, "{0}: {1}", name, value ? "true" : "false");
  } else {
    std::println(stderr, "{0}: {1}", name, value);
  }

  if constexpr (sizeof...(Rest) > 0) { log_pairs(std::forward<Rest>(rest)...); }
}

// Unified logger class with builder pattern
template <typename... Args> class logger {
private:
  std::source_location _loc;
  const char *_header;
  const char *_header_color;

  // Content storage
  std::string _message;
  std::optional<std::tuple<Args...>> _vars;
  bool _has_message = false;

  // Optional components
  const char *_condition = nullptr;
  const char *_expression = nullptr;
  error _error_code = error::generic;
  std::function<void()> _context_fn = nullptr;

  // Display flags
  bool _show_file = false;
  bool _show_function = false;
  bool _show_condition = false;
  bool _show_expression = false;
  bool _show_error = false;
  bool _show_stacktrace = false;
  bool _has_context = false;
  size_t _stacktrace_skip = 2;
  size_t _stacktrace_depth = 4;
  size_t _path_depth = 0; // 0=filename only, 1=parent/filename, etc.

public:
  // Constructor for builder-only mode (no message or vars): fatal functions
  logger(const char *header, const char *color, std::source_location loc)
      : _loc(loc), _header(header), _header_color(color) {}

  // Constructor for message logging: debug("msg", args...)
  // Shows file+line by default
  template <typename... MsgArgs>
  logger(const char *header, const char *color, std::source_location loc,
         std::format_string<MsgArgs...> fmt, MsgArgs &&...msg_args)
      : _loc(loc), _header(header), _header_color(color),
        _message(std::format(fmt, std::forward<MsgArgs>(msg_args)...)),
        _has_message(true) {
    _show_file = true; // Show location by default
  }

  // Constructor for variable logging: print(x, y, z)
  // Uses vars_tag to explicitly select this constructor
  // Shows file+line by default
  logger(const char *header, const char *color, std::source_location loc,
         vars_tag, Args &&...args)
      : _loc(loc), _header(header), _header_color(color) {
    _vars.emplace(std::forward<Args>(args)...);
    _show_file = true; // Show location by default
  }

  // Builder methods
  logger &location(size_t path_depth = 0) noexcept {
    _show_file = true;
    _path_depth = path_depth;
    return *this;
  }

  logger &func() noexcept {
    _show_function = true;
    return *this;
  }

  logger &cond(const char *condition) noexcept {
    _condition = condition;
    _show_condition = true;
    return *this;
  }

  logger &expr(const char *expression) noexcept {
    _expression = expression;
    _show_expression = true;
    return *this;
  }

  logger &err(error e) noexcept {
    _error_code = e;
    _show_error = true;
    return *this;
  }

  logger &msg(const char *message) noexcept {
    _message = message;
    _has_message = true;
    return *this;
  }

  template <typename... CtxArgs> logger &ctx(CtxArgs &&...ctx_args) {
    _context_fn = [ctx_tuple = std::make_tuple(
                       std::forward<CtxArgs>(ctx_args)...)]() mutable {
      std::apply(
          [](auto &&...a) { log_pairs(std::forward<decltype(a)>(a)...); },
          std::move(ctx_tuple));
    };
    _has_context = true;
    return *this;
  }

  logger &stacktrace(size_t max_depth = 4, size_t skip = 2) noexcept {
    _show_stacktrace = true;
    _stacktrace_depth = max_depth;
    _stacktrace_skip = skip;
    return *this;
  }

private:
  // Helper: Build complete log line (header + location + content) on one line
  std::string build_log_line() {
    std::string line = std::format("{0}[{1}]{2}",
                                   _header_color, // 0: header color
                                   _header,       // 1: header text
                                   color::reset); // 2: reset

    // Add file location if requested (always dim/gray)
    if (_show_file) {
      line +=
          std::format("{0} {1}:{2}{3}",
                      color::dim,                                   // 0: dim
                      truncate_path(_loc.file_name(), _path_depth), // 1: file
                      _loc.line(),                                  // 2: line
                      color::reset);                                // 3: reset
    }

    // Add function name if requested (always dim/gray)
    if (_show_function) {
      line += std::format("{0} in {1}{2}",
                          color::dim,           // 0: dim
                          _loc.function_name(), // 1: function
                          color::reset);        // 2: reset
    }

    // Add message or variables on the same line
    if (_has_message) {
      // Always append message with quotes
      line += " \"" + _message + "\"";
    } else if (_vars.has_value()) {
      if constexpr (sizeof...(Args) > 0) {
        // Build variables string with | separator
        std::string vars_str = std::apply(
            [](auto &&...a) {
              return build_pairs_string(std::forward<decltype(a)>(a)...);
            },
            *_vars);
        if (!vars_str.empty()) { line += " " + vars_str; }
      }
    }

    return line;
  }

  // Helper: Print condition, expression, error fields
  void print_metadata() const noexcept {
    if (_show_condition && _condition) {
      std::println(stderr, "Condition: {0}", _condition); // 0: condition
    }
    if (_show_expression && _expression) {
      std::println(stderr, "Expression: {0}", _expression); // 0: expression
    }
    if (_show_error && _error_code != error::generic) {
      std::println(stderr, "Error: {0}", to_string(_error_code)); // 0: error
    }
  }

public:
  // RAII logging in destructor
  ~logger() noexcept {
    // Build header + location + message/vars on one line
    std::string line = build_log_line();
    std::println(stderr, "{0}", line);

    // Print metadata (condition/expression/error) on separate lines if present
    if (_show_condition || _show_expression || _show_error) {
      print_metadata();
    }

    // Print context if present (always multi-line)
    if (_has_context && _context_fn) { _context_fn(); }

    // Print stacktrace if requested
    if (_show_stacktrace) {
      dbglog::stacktrace(_stacktrace_skip, _stacktrace_depth);
    }
  }

  // Prevent copying
  logger(const logger &) = delete;
  logger &operator=(const logger &) = delete;
};

// Deduction guides
template <typename... MsgArgs>
logger(const char *, const char *, std::source_location,
       std::format_string<MsgArgs...>, MsgArgs &&...) -> logger<>;

template <typename... Args>
logger(const char *, const char *, std::source_location, vars_tag, Args &&...)
    -> logger<Args...>;

} // namespace dbglog

// #define DISABLE_DEBUG_LOG;
#ifndef DISABLE_DEBUG_LOG

// C++20 __VA_OPT__ based recursive macro expansion
#define PARENS ()

// Exponential expansion - enables deep recursion (up to 4^4 = 256 levels)
#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

// FOR_EACH applies a macro to each argument in __VA_ARGS__
#define FOR_EACH_HELPER(macro, a1, ...)                                        \
  macro(a1) __VA_OPT__(, FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER
#define FOR_EACH(macro, ...)                                                   \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))

// Stringify helper - converts 'x' to "#x", x
#define __DEBUG_PAIR(x) #x, x

// print(x, y, z) -> creates logger for variables, shows file+line by default
// optionally chain .func().ctx()
#define print(...)                                                             \
  dbglog::logger("Debug", dbglog::color::dim, std::source_location::current(), \
                 dbglog::vars, FOR_EACH(__DEBUG_PAIR, __VA_ARGS__))

// debug("message") or debug("format: {}", value) -> creates logger for
// messages, shows file+line by default, optionally chain .func().ctx()
#define debug(...)                                                             \
  dbglog::logger("Debug", dbglog::color::dim, std::source_location::current(), \
                 __VA_ARGS__)

#else
// Debug logging disabled - compiles to nothing
#define print(...) ((void)0)
#define debug(...) ((void)0)
#endif // DISABLE_DEBUG_LOG
