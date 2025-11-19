#pragma once
#include <cstdlib>
#include <expected>
#include <functional>
#include <result/error.h>
#include <result/log.h>
#include <source_location>
#include <utility>

#define exforward(x) ::std::forward<decltype(x)>(x)

template <typename T = void> struct result : public std::expected<T, error> {
  using base = std::expected<T, error>;
  using base::base;
  constexpr result(error e) noexcept : base(std::unexpected(e)) {}

  result(const char *msg,
         std::source_location loc = std::source_location::current()) noexcept
      : base(std::unexpected(error::generic)) {
    {
      dbglog::logger<>("Fail: Unconditional", dbglog::color::yellow, loc)
          .location()
          .msg(msg)
          .err(error::generic);
    }
  }
};

template <typename T>
struct result<T &> : public std::expected<std::reference_wrapper<T>, error> {
  using base = std::expected<std::reference_wrapper<T>, error>;
  using base::base;

  constexpr result(T &ref) noexcept : base(std::ref(ref)) {}
  constexpr result(error e) noexcept : base(std::unexpected(e)) {}

  result(const char *msg,
         std::source_location loc = std::source_location::current()) noexcept
      : base(std::unexpected(error::generic)) {
    {
      dbglog::logger<>("Fail: Unconditional", dbglog::color::yellow, loc)
          .location()
          .msg(msg)
          .err(error::generic);
    }
  }

  constexpr T &value(this auto &&self) {
    return exforward(self).base::value().get();
  }
  constexpr decltype(auto) operator*(this auto &&self) {
    return exforward(self).value();
  }
  constexpr T *operator->() { return &value(); }
  constexpr const T *operator->() const { return &value(); }
};

decltype(auto) try_unwrap_impl(auto &&result, std::false_type x) {
  return std::forward<decltype(result)>(result).value();
}

struct void_result_tag {};
void_result_tag try_unwrap_impl(auto &&result, std::true_type x) {
  return void_result_tag{};
}

template <typename T> decltype(auto) try_unwrap(T &&result) {
  using value_type = typename std::decay_t<T>::value_type;
  return try_unwrap_impl(std::forward<T>(result), std::is_void<value_type>{});
}

// wrapper to tag pointers created from references
template <typename T> struct ptr_from_ref {
  T *ptr;
};

// return tagged pointer for references, pass through everything else
template <typename T> auto ok_unwrap_helper(T &&value) {
  using raw_t = std::remove_reference_t<T>;
  if constexpr (std::is_reference_v<T> && !std::is_pointer_v<raw_t>) {
    return ptr_from_ref<raw_t>{&value};
  } else {
    return std::forward<T>(value);
  }
}

// Handle void results by passing through the tag
inline void_result_tag ok_unwrap_helper(void_result_tag tag) { return tag; }

template <typename> struct is_ptr_from_ref : std::false_type {};
template <typename T>
struct is_ptr_from_ref<ptr_from_ref<T>> : std::true_type {};

// dereference tagged pointers, pass through everything else
template <typename T> decltype(auto) ok_deref_helper(T &&value) {
  if constexpr (is_ptr_from_ref<T>::value) {
    return *value.ptr;
  } else {
    return std::forward<T>(value);
  }
}

// Convert void result tag back to void
inline void ok_deref_helper(void_result_tag) {}

// possible names: ok, test, ret, flow, get, do, run, ok, out
#define ok(expr)                                                               \
  ok_deref_helper(({                                                           \
    auto &&__result = (expr);                                                  \
    if (!__result) { return std::unexpected(__result.error()); }               \
    ok_unwrap_helper(try_unwrap(exforward(__result)));                         \
  }))

__attribute__((always_inline)) inline void fatal_error(
    const char *expr, error err,
    std::source_location loc = std::source_location::current()) noexcept {
  {
    dbglog::logger<>("Fatal: Result unwrap failed", dbglog::color::red, loc)
        .location()
        .expr(expr)
        .err(err)
        .stacktrace(2);
  } // Logger destructs here, outputs the log
  std::abort();
}

#define unwrap(expr)                                                           \
  ({                                                                           \
    auto &&__result = (expr);                                                  \
    if (!__result.has_value()) { fatal_error(#expr, __result.error()); }       \
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
  bool _has_context = false;
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
  template <typename... Args> fail_builder &&ctx(Args &&...args) && {
    _context_logger =
        [args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
          std::apply(
              [](auto &&...a) {
                dbglog::log_pairs(std::forward<decltype(a)>(a)...);
              },
              std::move(args_tuple));
        };
    _has_context = true;
    return std::move(*this);
  }

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
      // Build logger with all options
      if (_message && _log_stacktrace) {
        dbglog::logger<>("Fail", dbglog::color::yellow, _loc)
            .location()
            .cond(_condition)
            .err(_error_code)
            .msg(_message)
            .stacktrace(0);
      } else if (_message) {
        dbglog::logger<>("Fail", dbglog::color::yellow, _loc)
            .location()
            .cond(_condition)
            .err(_error_code)
            .msg(_message);
      } else if (_log_stacktrace) {
        dbglog::logger<>("Fail", dbglog::color::yellow, _loc)
            .location()
            .cond(_condition)
            .err(_error_code)
            .stacktrace(0);
      } else {
        dbglog::logger<>("Fail", dbglog::color::yellow, _loc)
            .location()
            .cond(_condition)
            .err(_error_code);
      }

      // Call context logger after main logging
      if (_has_context && _context_logger) { _context_logger(); }
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
  {
    dbglog::logger<>("Fatal", dbglog::color::red, loc)
        .location()
        .cond(condition_str)
        .msg(message)
        .stacktrace(2);
  } // Logger destructs here, outputs the log
  std::abort();
}

#define __fatal_1(condition)                                                   \
  ({                                                                           \
    if (condition) [[unlikely]] { fatal_assertion(#condition, #condition); }   \
  })

#define __fatal_2(condition, message)                                          \
  ({                                                                           \
    if (condition) [[unlikely]] { fatal_assertion(#condition, message); }      \
  })

#define __fatal_GET_MACRO(_1, _2, NAME, ...) NAME
#define fatal(...)                                                             \
  __fatal_GET_MACRO(__VA_ARGS__, __fatal_2, __fatal_1)(__VA_ARGS__)

template <typename T> constexpr void *to_nullptr(result<T *> res) noexcept {
  return res.value_or(nullptr);
}

template <typename T> constexpr void *to_nullptr(result<T &> res) noexcept {
  if (!res) { return nullptr; }
  return &res.value();
}

template <typename T> constexpr void *to_nullptr(result<T> res) noexcept {
  if (!res) { return nullptr; }
  return static_cast<void *>(res.value());
}
