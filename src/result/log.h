#pragma once
#include <print>
#include <result/error.h>
#include <source_location>
#include <stacktrace>
#include <string>
#include <string_view>
#include <type_traits>

namespace log {

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

inline void header(const char *msg) noexcept {
  const char *clr =
      (std::string_view(msg).find("fatal") != std::string_view::npos)
          ? color::red
          : color::yellow;
  std::println(stderr, "{}{}{}", clr, msg, color::reset);
}

inline std::string_view truncate_path(std::string_view path) noexcept {
  // Find last '/' from the end
  auto last_slash = path.rfind('/');
  if (last_slash == std::string_view::npos) { return path; }

  // Find second-to-last '/' from the end
  auto second_last = path.rfind('/', last_slash - 1);
  if (second_last == std::string_view::npos) { return path; }

  // Return substring from second-to-last slash onward (skip the slash itself)
  return path.substr(second_last + 1);
}

inline void debug_header() noexcept {
  std::println(stderr, "{}[Debug]{}", color::dim, color::reset);
}

inline void debug_header_with_location(std::source_location loc) noexcept {
  std::println(stderr, "{}[Debug] {} {}:{}", color::dim,
               truncate_path(loc.file_name()), loc.line(), color::reset);
}

inline void location(std::source_location loc) noexcept {
  std::println(stderr, "Location:\t{}:{}", loc.file_name(), loc.line());
  // std::println(stderr, "{}Function:   {}{}", color::dim, loc.function_name(),
  // color::reset);
}

inline void expression(const char *expr) noexcept {
  std::println(stderr, "Expression:\t{}", expr);
}

inline void condition(const char *cond) noexcept {
  std::println(stderr, "Condition:\t{}", cond);
}

inline void error_code(error err) noexcept {
  if (err != error::generic) {
    std::println(stderr, "Error:\t{}", to_string(err));
  }
}

inline void message(const char *msg) noexcept {
  std::println(stderr, "Message:\t\"{}\"", msg);
}

inline void stacktrace(std::size_t skip_frames = 0,
                       std::size_t max_depth = 20) noexcept {
  std::println(stderr, "Stack trace:");
  std::print(stderr, "{}\n",
             std::to_string(std::stacktrace::current(skip_frames, max_depth)));
}

inline void log_pairs() noexcept {}

template <typename First, typename Second, typename... Rest>
inline void log_pairs(First &&name, Second &&value, Rest &&...rest) noexcept {
  if constexpr (std::is_pointer_v<std::decay_t<Second>>) {
    std::println(stderr, "  {}: {}", name, static_cast<const void *>(value));
  } else if constexpr (std::is_same_v<std::decay_t<Second>, bool>) {
    std::println(stderr, "  {}: {}", name, value ? "true" : "false");
  } else {
    std::println(stderr, "  {}: {}", name, value);
  }

  if constexpr (sizeof...(Rest) > 0) { log_pairs(std::forward<Rest>(rest)...); }
}

template <typename... Args>
inline void debug_impl(std::source_location loc, Args &&...args) noexcept {
  debug_header_with_location(loc);
  if constexpr (sizeof...(Args) > 0) { log_pairs(std::forward<Args>(args)...); }
}

template <typename... Args>
inline void debug_msg_impl(std::source_location loc,
                           std::format_string<Args...> fmt,
                           Args &&...args) noexcept {
  debug_header_with_location(loc);
  std::println(stderr, "{}", std::format(fmt, std::forward<Args>(args)...));
}

} // namespace log

// #define DISABLE_DEBUG_LOG;
// Debug logging can be disabled at compile time by defining DISABLE_DEBUG_LOG
// Example: g++ -DDISABLE_DEBUG_LOG ...
#ifndef DISABLE_DEBUG_LOG

// C++20 __VA_OPT__ based recursive macro expansion
// Enables processing of unlimited arguments without boilerplate
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

// debug(x, y, z) -> log::debug_impl(location, "x", x, "y", y, "z", z)
#define debug(...)                                                             \
  log::debug_impl(std::source_location::current(),                             \
                  FOR_EACH(__DEBUG_PAIR, __VA_ARGS__))

// log("message") or log("format: {}", value)
#define log(...)                                                               \
  log::debug_msg_impl(std::source_location::current(), __VA_ARGS__)

#else
// Debug logging disabled - compiles to nothing
#define debug(...) ((void)0)
#define log(...) ((void)0)
#endif // DISABLE_DEBUG_LOG
