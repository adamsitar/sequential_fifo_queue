#pragma once
#include <cstdlib>
#include <error.h>
#include <expected>
#include <functional>
#include <log.h>
#include <source_location>
#include <utility>

template <typename T = void> struct result : public std::expected<T, error> {
  using base = std::expected<T, error>;
  using base::base;
  constexpr result(error e) noexcept : base(std::unexpected(e)) {}

  result(const char *msg,
         std::source_location loc = std::source_location::current()) noexcept
      : base(std::unexpected(error::generic)) {
    log::header("[fail] Unconditional");
    log::location(loc);
    log::message(msg);
    log::error_code(error::generic);
  }
};

auto try_unwrap_impl(auto &&result, std::false_type x) {
  return std::forward<decltype(result)>(result).value();
}

void try_unwrap_impl(auto &&result, std::true_type x) {}

template <typename T> auto try_unwrap(T &&result) {
  using value_type = typename std::decay_t<T>::value_type;
  return try_unwrap_impl(std::forward<T>(result), std::is_void<value_type>{});
}

#define TRY(expr)                                                              \
  ({                                                                           \
    auto &&__result = (expr);                                                  \
    if (!__result) {                                                           \
      return std::unexpected(__result.error());                                \
    }                                                                          \
    try_unwrap(std::forward<decltype(__result)>(__result));                    \
  })

__attribute__((always_inline)) inline void fatal_error(
    const char *expr, error err,
    std::source_location loc = std::source_location::current()) noexcept {
  log::header("FATAL: Result unwrap failed");
  log::location(loc);
  log::expression(expr);
  log::error_code(err);
  log::stacktrace(2); // Skip log::stacktrace() and fatal_error()
  std::abort();
}

#define unwrap(expr)                                                           \
  ({                                                                           \
    auto &&__result = (expr);                                                  \
    if (!__result.has_value()) {                                               \
      fatal_error(#expr, __result.error());                                    \
    }                                                                          \
    try_unwrap(std::forward<decltype(__result)>(__result));                    \
  })

#define VAR(x) #x, x

class [[nodiscard]] fail_builder {
private:
  const char *_condition;
  std::source_location _loc;
  error _error_code = error::generic;
  const char *_message;
  std::function<void()> _context_logger = nullptr;
  // bool _has_context = false;
  bool _log_stacktrace = false;
  bool _silent = false;

public:
  // fail(condition, "message")
  constexpr fail_builder(const char *cond, std::source_location loc,
                         const char *msg) noexcept
      : _condition(cond), _loc(loc), _message(msg) {}

  // fail(condition, error::code)
  constexpr fail_builder(const char *cond, std::source_location loc,
                         error e) noexcept
      : _condition(cond), _loc(loc), _error_code(e), _message(nullptr) {}

  constexpr fail_builder &&err(error e) && noexcept {
    _error_code = e;
    return std::move(*this);
  }

  // WIP
  //  template <typename... Args> fail_builder &&ctx(Args &&...args) && {
  //    _context_logger = [args_tuple = std::make_tuple(
  //                           std::forward<Args>(args)...)]() mutable {
  //      std::apply(
  //          [](auto &&...a) { log::log_pairs(std::forward<decltype(a)>(a)...);
  //          }, std::move(args_tuple));
  //    };
  //    _has_context = true;
  //    return std::move(*this);
  //  }

  constexpr fail_builder &&stacktrace() && noexcept {
    _log_stacktrace = true;
    return std::move(*this);
  }

  constexpr fail_builder &&silent() && noexcept {
    _silent = true;
    return std::move(*this);
  }

  template <typename T = void> operator result<T>() && noexcept {
    if (!_silent) {
      log::header("[fail]");
      log::location(_loc);
      log::condition(_condition);
      log::error_code(_error_code);
      if (_message) {
        log::message(_message);
      }
      // if (_has_context && _context_logger) {
      //   _context_logger();
      // }
      if (_log_stacktrace) {
        log::stacktrace(2);
      }
    }
    return std::unexpected(_error_code);
  }

  fail_builder(const fail_builder &) = delete;
  fail_builder &operator=(const fail_builder &) = delete;
};

#define __fail_1(condition)                                                    \
  if ((condition)) [[unlikely]]                                                \
  return fail_builder(#condition, std::source_location::current(), nullptr)

#define __fail_2(condition, message)                                           \
  if ((condition)) [[unlikely]]                                                \
  return fail_builder(#condition, std::source_location::current(), message)

#define __fail_GET_MACRO(_1, _2, NAME, ...) NAME
#define fail(...) __fail_GET_MACRO(__VA_ARGS__, __fail_2, __fail_1)(__VA_ARGS__)

__attribute__((always_inline)) inline void fatal_assertion(
    const char *condition_str, const char *message,
    std::source_location loc = std::source_location::current()) noexcept {
  log::header("[fatal]");
  log::location(loc);
  log::condition(condition_str);
  log::message(message);
  log::stacktrace(2); // Skip log::stacktrace() and fatal_assertion()
  std::abort();
}

#define __fatal_1(condition)                                                   \
  ({                                                                           \
    if (condition) [[unlikely]] {                                              \
      fatal_assertion(#condition, #condition);                                 \
    }                                                                          \
  })

#define __fatal_2(condition, message)                                          \
  ({                                                                           \
    if (condition) [[unlikely]] {                                              \
      fatal_assertion(#condition, message);                                    \
    }                                                                          \
  })

#define __fatal_GET_MACRO(_1, _2, NAME, ...) NAME
#define fatal(...)                                                             \
  __fatal_GET_MACRO(__VA_ARGS__, __fatal_2, __fatal_1)(__VA_ARGS__)

template <typename T> constexpr void *to_nullptr(result<T *> res) noexcept {
  return res.value_or(nullptr);
}

template <typename T> constexpr void *to_nullptr(result<T &> res) noexcept {
  if (!res) {
    return nullptr;
  }
  return &res.value();
}

template <typename T> constexpr void *to_nullptr(result<T> res) noexcept {
  if (!res) {
    return nullptr;
  }
  return static_cast<void *>(res.value());
}
