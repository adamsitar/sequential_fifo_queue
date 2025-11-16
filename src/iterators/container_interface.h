#pragma once
#include <iterator>

// ============================================================================
// Forward Container Iterator Interface (CRTP)
// ============================================================================
// Provides: cbegin(), cend()
// Requires: Derived must implement begin() and end()
//
// Use for: forward-only containers (singly-linked lists, etc.)
// ============================================================================

template <typename derived> class forward_iterator_interface {
public:
  // cbegin/cend work on both const and non-const containers, always return
  // const iterators
  constexpr auto cbegin(this auto &self) noexcept {
    using iterator = decltype(self.begin());
    using const_iterator = std::const_iterator<iterator>;
    return const_iterator(self.begin());
  }

  constexpr auto cend(this auto &self) noexcept {
    using iterator = decltype(self.end());
    using const_iterator = std::const_iterator<iterator>;
    return const_iterator(self.end());
  }
};

// ============================================================================
// Bidirectional Container Iterator Interface (CRTP)
// ============================================================================
// Adds: rbegin(), rend(), crbegin(), crend()
// Requires: Same as forward_container_iterator_interface
//
// Use for: bidirectional/random-access containers (arrays, deques, etc.)
// ============================================================================

template <typename derived>
class bidirectional_iterator_interface
    : public forward_iterator_interface<derived> {
public:
  constexpr auto rbegin(this auto &self) noexcept {
    using iterator = decltype(self.begin());
    using reverse_iterator = std::reverse_iterator<iterator>;
    return reverse_iterator(self.end());
  }

  constexpr auto rend(this auto &self) noexcept {
    using iterator = decltype(self.begin());
    using reverse_iterator = std::reverse_iterator<iterator>;
    return reverse_iterator(self.begin());
  }

  constexpr auto crbegin(this auto &self) noexcept {
    using reverse_iterator = decltype(self.rbegin());
    using const_reverse_iterator = std::const_iterator<reverse_iterator>;
    return const_reverse_iterator(self.rbegin());
  }

  constexpr auto crend(this auto &self) noexcept {
    using reverse_iterator = decltype(self.rend());
    using const_reverse_iterator = std::const_iterator<reverse_iterator>;
    return const_reverse_iterator(self.rend());
  }
};

// ============================================================================
// Before-Begin Iterator Mixin (CRTP)
// ============================================================================
// Provides: cbefore_begin()
// Requires: Derived must implement before_begin()
//
// Use for: forward_list-style containers that support insertion before front
//
// Note: This is a separate mixin because before_begin is non-standard
// (only std::forward_list has it). Compose with forward_container_interface:
//
// class my_list : public forward_container_iterator_interface<my_list>,
//                 public before_begin_iterator_interface<my_list> { ... };
// ============================================================================

template <typename derived> class before_begin_iterator_interface {
public:
  constexpr auto cbefore_begin(this auto &self) noexcept {
    using iterator = decltype(self.before_begin());
    using const_iterator = std::const_iterator<iterator>;
    return const_iterator(self.before_begin());
  }
};
