#pragma once
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>

// ============================================================================
// Input Iterator Facade
// ============================================================================
// Derived class must implement:
//   - reference dereference() const
//   - void increment()
//   - bool equals(const Derived&) const
//
// Provides: *, ->, ++, ++(int), ==, !=, typedefs
// Use for: Single-pass read-only iterators (e.g., input streams)
// ============================================================================

template <typename value, typename reference_t = value &,
          typename pointer_t = value *>
class input_iterator_facade {
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = std::remove_cv_t<value>;
  using difference_type = std::ptrdiff_t;
  using pointer = pointer_t;
  using reference = reference_t;

  constexpr reference operator*(this auto const &self) noexcept {
    return self.dereference();
  }

  constexpr pointer operator->(this auto const &self) noexcept {
    if constexpr (std::is_pointer_v<reference>) {
      return self.dereference();
    } else {
      return &self.dereference();
    }
  }

  constexpr auto &operator++(this auto &self) noexcept {
    self.increment();
    return self;
  }

  constexpr auto operator++(this auto &self, int) noexcept {
    auto tmp = self;
    self.increment();
    return tmp;
  }

  constexpr bool operator==(this auto const &self, auto const &other) noexcept {
    return self.equals(other);
  }
};

// ============================================================================
// Output Iterator Facade
// ============================================================================
// Derived class must implement:
//   - reference dereference() const  (typically returns proxy)
//   - void increment()
//
// Provides: *, ++, ++(int), typedefs
// Use for: Single-pass write-only iterators (e.g., output streams)
// Note: Output iterators don't need equals() - no comparison needed
// ============================================================================

template <typename value> class output_iterator_facade {
public:
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using difference_type = std::ptrdiff_t;
  using pointer = void;
  using reference = void;

  constexpr decltype(auto) operator*(this auto &self) noexcept {
    return self.dereference();
  }

  constexpr auto &operator++(this auto &self) noexcept {
    self.increment();
    return self;
  }

  constexpr auto operator++(this auto &self, int) noexcept {
    auto tmp = self;
    self.increment();
    return tmp;
  }
};

// ============================================================================
// Forward Iterator Facade
// ============================================================================
// Derived class must implement:
//   - reference dereference() const
//   - void increment()
//   - bool equals(const Derived&) const
//
// Provides: *, ->, ++, ++(int), ==, !=, typedefs
// ============================================================================

template <typename value, typename reference_t = value &,
          typename pointer_t = value *>
class forward_iterator_facade {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = std::remove_cv_t<value>;
  using difference_type = std::ptrdiff_t;
  using pointer = pointer_t;
  using reference = reference_t;

  constexpr reference operator*(this auto const &self) noexcept {
    return self.dereference();
  }

  constexpr pointer operator->(this auto const &self) noexcept {
    if constexpr (std::is_pointer_v<reference>) {
      return self.dereference();
    } else {
      return &self.dereference();
    }
  }

  constexpr auto &operator++(this auto &self) noexcept {
    self.increment();
    return self;
  }

  constexpr auto operator++(this auto &self, int) noexcept {
    auto tmp = self;
    self.increment();
    return tmp;
  }

  constexpr bool operator==(this auto const &self, auto const &other) noexcept {
    return self.equals(other);
  }
};

// ============================================================================
// Bidirectional Iterator Facade (C++23 Deducing This)
// ============================================================================
// Adds to forward_iterator_facade:
//   - void decrement()
//
// Provides: --, --(int)
// ============================================================================

template <typename value, typename reference = value &,
          typename pointer = value *>
class bidirectional_iterator_facade
    : public forward_iterator_facade<value, reference, pointer> {
public:
  using iterator_category = std::bidirectional_iterator_tag;

  constexpr auto &operator--(this auto &self) noexcept {
    self.decrement();
    return self;
  }

  constexpr auto operator--(this auto &self, int) noexcept {
    auto tmp = self;
    self.decrement();
    return tmp;
  }
};

// ============================================================================
// Random Access Iterator Facade (C++23 Deducing This)
// ============================================================================
// Adds to bidirectional_iterator_facade:
//   - void advance(difference_type n)
//   - difference_type distance_to(const Derived&) const
//
// Provides: +, -, +=, -=, [], <, >, <=, >=, <=>
// ============================================================================

template <typename value, typename reference = value &,
          typename pointer = value *>
class random_access_iterator_facade
    : public bidirectional_iterator_facade<value, reference, pointer> {
public:
  using iterator_category = std::random_access_iterator_tag;
  using difference_type = std::ptrdiff_t;

  constexpr auto &operator+=(this auto &self, difference_type n) noexcept {
    self.advance(n);
    return self;
  }

  constexpr auto &operator-=(this auto &self, difference_type n) noexcept {
    self.advance(-n);
    return self;
  }

  constexpr auto operator+(this auto const &self, difference_type n) noexcept {
    auto tmp = self;
    tmp.advance(n);
    return tmp;
  }

  constexpr auto operator-(this auto const &self, difference_type n) noexcept {
    auto tmp = self;
    tmp.advance(-n);
    return tmp;
  }

  constexpr difference_type operator-(this auto const &self,
                                      auto const &other) noexcept {
    return -other.distance_to(self);
  }

  constexpr decltype(auto) operator[](this auto const &self,
                                      difference_type n) noexcept {
    auto tmp = self;
    tmp.advance(n);
    return *tmp;
  }

  constexpr std::strong_ordering operator<=>(this auto const &self,
                                             auto const &other) noexcept {
    auto dist = self.distance_to(other);
    if (dist < 0) { return std::strong_ordering::less; }
    if (dist > 0) { return std::strong_ordering::greater; }
    return std::strong_ordering::equal;
  }
};

// ============================================================================
// Non-member operator+ for symmetry (n + iterator)
// ============================================================================

template <typename T>
  requires std::derived_from<T, random_access_iterator_facade<
                                    typename T::value_type,
                                    typename T::reference, typename T::pointer>>
constexpr T operator+(typename T::difference_type n, const T &it) noexcept {
  return it + n;
}

// ============================================================================
// Contiguous Iterator Facade (C++20)
// ============================================================================
// Adds to random_access_iterator_facade:
//   - Value* to_address() const
//
// Provides: std::to_address support for contiguous memory
// Use for: Iterators over contiguous memory (arrays, vectors, etc.)
// ============================================================================

template <typename Value, typename Reference = Value &,
          typename Pointer = Value *>
class contiguous_iterator_facade
    : public random_access_iterator_facade<Value, Reference, Pointer> {
public:
  using iterator_category = std::contiguous_iterator_tag;
  using element_type = Value;

  // Enable std::to_address()
  constexpr Value *operator->(this auto const &self) noexcept {
    return self.to_address();
  }
};

// ============================================================================
// Helper: Override decrement() for random access to use advance(-1)
// ============================================================================
// Just add this to your random access iterator class:
//   void decrement() { advance(-1); }
// ============================================================================
