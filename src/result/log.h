#pragma once
#include <result/error.h>
#include <print>
#include <source_location>
#include <stacktrace>
#include <string>
#include <type_traits>

namespace log {

// ANSI color codes
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
  // Color based on severity: [fatal] = red, [fail] = yellow
  const char *clr =
      (std::string_view(msg).find("fatal") != std::string_view::npos)
          ? color::red
          : color::yellow;
  std::println(stderr, "{}{}{}", clr, msg, color::reset);
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
  // Only log non-generic errors (generic doesn't add information)
  if (err != error::generic) {
    std::println(stderr, "Error:\t{}", to_string(err));
  }
}

inline void message(const char *msg) noexcept {
  std::println(stderr, "Message:\t\"{}\"", msg);
}

inline void stacktrace(std::size_t skip_frames = 0,
                       std::size_t max_depth = 7) noexcept {
  std::println(stderr, "Stack trace:");
  std::print(stderr, "{}\n",
             std::to_string(std::stacktrace::current(skip_frames, max_depth)));
}

inline void log_pairs() noexcept {}

// Recursive case
template <typename First, typename Second, typename... Rest>
inline void log_pairs(First &&name, Second &&value, Rest &&...rest) noexcept {
  // Type-appropriate formatting
  if constexpr (std::is_pointer_v<std::decay_t<Second>>) {
    std::println(stderr, "  {}: {}", name, static_cast<const void *>(value));
  } else if constexpr (std::is_same_v<std::decay_t<Second>, bool>) {
    std::println(stderr, "  {}: {}", name, value ? "true" : "false");
  } else {
    std::println(stderr, "  {}: {}", name, value);
  }

  if constexpr (sizeof...(Rest) > 0) {
    log_pairs(std::forward<Rest>(rest)...);
  }
}

} // namespace log
