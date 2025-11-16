#pragma once
#include <cstddef>

template <typename T, typename unique_tag, typename tag> struct cache {
  inline static T _value{};

  static T get() noexcept { return _value; }
  static void set(T value) noexcept { _value = value; }
  static void reset() noexcept { _value = T{}; }
};

template <typename unique_tag>
using alloc_hint_cache = cache<uint8_t, unique_tag, decltype([] {})>;

template <typename unique_tag>
using lookup_hint_cache = cache<uint8_t, unique_tag, decltype([] {})>;
