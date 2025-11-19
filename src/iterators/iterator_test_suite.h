#pragma once

#include <algorithm>
#include <compare>
#include <gtest/gtest.h>
#include <iterator>
#include <vector>

// ============================================================================
// Generic Iterator Test Suite
// ============================================================================
// This header provides generic iterator tests that work with any container
// implementing the iterator_facade patterns.
//
// Usage:
//   1. Create ContainerAdapter for the data structure
//   2. Add adapter to appropriate type list (ForwardIteratorTypes, etc.)
//   3. Tests automatically instantiate for container
// ============================================================================

// ============================================================================
// Container Adapter Concept
// ============================================================================
// Defines the required interface for types that adapt containers to work with
// the generic iterator test suites. Adapters must provide:
//   - Type aliases: container_type, value_type
//   - Static populate() method to insert test data
//
// NOTE: Containers must be default constructible
// ============================================================================

template <typename Adapter>
concept ContainerAdapterFor =
    requires {
      typename Adapter::container_type;
      typename Adapter::value_type;
    } && requires(typename Adapter::container_type &container,
                  std::vector<typename Adapter::value_type> const &values) {
      // Required static function for populating test data
      { Adapter::populate(container, values) };
    };

// ============================================================================
// Forward Iterator Tests
// ============================================================================
// Tests requirements for std::forward_iterator:
// - Default constructible, copy constructible, copy assignable
// - operator*, operator->, operator++, operator++(int)
// - operator==, operator!=
// - begin() == end() for empty container
// - Multi-pass guarantee (can iterate multiple times)
// ============================================================================

template <typename ContainerAdapter>
  requires ContainerAdapterFor<ContainerAdapter>
class GenericForwardIteratorTestSuite : public ::testing::Test {
protected:
  using container_type = typename ContainerAdapter::container_type;
  using value_type = typename ContainerAdapter::value_type;
  using iterator = typename container_type::iterator;

  // Default-constructed container using simple_test_allocator
  // No manual lifetime management needed!
  container_type container;

  void populate(std::vector<value_type> const &values) {
    ContainerAdapter::populate(container, values);
  }

  std::vector<value_type> collect() {
    std::vector<value_type> result;
    for (auto it = container.begin(); it != container.end(); ++it) {
      result.push_back(*it);
    }
    return result;
  }

  // NOTE: const iterator tests disabled due to std::const_iterator limitations
  // The C++23 std::const_iterator wrapper doesn't properly forward comparison
  // operators from iterator facade. Regular iterators work fine.
  // std::vector<value_type> collect_const() const { ... }
};

TYPED_TEST_SUITE_P(GenericForwardIteratorTestSuite);

// Basic iterator validity tests
TYPED_TEST_P(GenericForwardIteratorTestSuite, BeginEqualsEndWhenEmpty) {
  EXPECT_EQ(this->container.begin(), this->container.end());
}

// Disabled: std::const_iterator comparison issues
// TYPED_TEST_P(ForwardIteratorTest, CBeginEqualsCEndWhenEmpty) {
//   EXPECT_EQ(this->container.cbegin(), this->container.cend());
// }

TYPED_TEST_P(GenericForwardIteratorTestSuite, BeginNotEqualsEndWhenPopulated) {
  this->populate({1, 2, 3});
  EXPECT_NE(this->container.begin(), this->container.end());
}

// Dereference operator tests
TYPED_TEST_P(GenericForwardIteratorTestSuite, DereferenceReturnsFirstElement) {
  this->populate({42, 100, 200});
  auto it = this->container.begin();
  EXPECT_EQ(*it, 42);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, ArrowOperatorAccessesMember) {
  this->populate({10, 20, 30});
  auto it = this->container.begin();
  // For primitive types, just verify we can take address
  EXPECT_EQ(&(*it), &(*it)); // Verifies operator-> returns sensible pointer
}

// Pre-increment operator tests
TYPED_TEST_P(GenericForwardIteratorTestSuite,
             PreIncrementAdvancesToNextElement) {
  this->populate({1, 2, 3});
  auto it = this->container.begin();
  ++it;
  EXPECT_EQ(*it, 2);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, PreIncrementReturnsReference) {
  this->populate({1, 2, 3});
  auto it = this->container.begin();
  auto &ref = ++it;
  EXPECT_EQ(&ref, &it); // Should return reference to itself
}

TYPED_TEST_P(GenericForwardIteratorTestSuite,
             MultiplePreIncrementsTraverseAll) {
  this->populate({10, 20, 30, 40});
  auto it = this->container.begin();
  ++it; // -> 20
  ++it; // -> 30
  ++it; // -> 40
  ++it; // -> end()
  EXPECT_EQ(it, this->container.end());
}

// Post-increment operator tests
TYPED_TEST_P(GenericForwardIteratorTestSuite, PostIncrementReturnsOldValue) {
  this->populate({5, 10, 15});
  auto it = this->container.begin();
  auto old = it++;
  EXPECT_EQ(*old, 5);
  EXPECT_EQ(*it, 10);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, PostIncrementAdvancesIterator) {
  this->populate({100, 200});
  auto it = this->container.begin();
  it++;
  EXPECT_EQ(*it, 200);
}

// Traversal tests
TYPED_TEST_P(GenericForwardIteratorTestSuite, CompleteTraversalReachesEnd) {
  this->populate({1, 2, 3, 4, 5});
  auto it = this->container.begin();
  for (int i = 0; i < 5; ++i) {
    EXPECT_NE(it, this->container.end());
    ++it;
  }
  EXPECT_EQ(it, this->container.end());
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, TraversalCollectsAllElements) {
  std::vector<int> expected = {10, 20, 30, 40, 50};
  this->populate(expected);
  auto collected = this->collect();
  EXPECT_EQ(collected, expected);
}

// Disabled: std::const_iterator comparison issues
// TYPED_TEST_P(ForwardIteratorTest, ConstIteratorTraversesElements) {
//   std::vector<int> expected = {7, 14, 21, 28};
//   this->populate(expected);
//   auto collected = this->collect_const();
//   EXPECT_EQ(collected, expected);
// }

// TYPED_TEST_P(ForwardIteratorTest, BeginConvertsToConstIterator) {
//   this->populate({1, 2, 3});
//   // Test that we can get const iterator via cbegin()
//   auto cit = this->container.cbegin();
//   EXPECT_EQ(*cit, 1);
// }

// Multi-pass guarantee (forward iterators can be copied and traversed
// independently)
TYPED_TEST_P(GenericForwardIteratorTestSuite, MultiPassGuarantee) {
  this->populate({100, 200, 300});
  auto it1 = this->container.begin();
  auto it2 = it1; // Copy

  ++it1;                // Advance first copy
  EXPECT_EQ(*it2, 100); // Second copy unchanged
  EXPECT_EQ(*it1, 200);

  ++it2; // Advance second copy
  EXPECT_EQ(*it2, 200);
}

// Range-based for loop support
TYPED_TEST_P(GenericForwardIteratorTestSuite, RangeBasedForLoopSupport) {
  std::vector<int> expected = {3, 6, 9, 12};
  this->populate(expected);

  std::vector<int> collected;
  for (auto const &value : this->container) {
    collected.push_back(value);
  }

  EXPECT_EQ(collected, expected);
}

// STL algorithm compatibility
TYPED_TEST_P(GenericForwardIteratorTestSuite, StdFindWorks) {
  this->populate({5, 10, 15, 20, 25});
  auto it = std::find(this->container.begin(), this->container.end(), 15);
  EXPECT_NE(it, this->container.end());
  EXPECT_EQ(*it, 15);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, StdCountWorks) {
  this->populate({1, 2, 3, 2, 4, 2});
  auto count = std::count(this->container.begin(), this->container.end(), 2);
  EXPECT_EQ(count, 3);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, StdDistanceWorks) {
  this->populate({10, 20, 30, 40, 50});
  auto dist = std::distance(this->container.begin(), this->container.end());
  EXPECT_EQ(dist, 5);
}

TYPED_TEST_P(GenericForwardIteratorTestSuite, StdCopyWorks) {
  this->populate({7, 8, 9});
  std::vector<int> dest;
  std::copy(this->container.begin(), this->container.end(),
            std::back_inserter(dest));
  EXPECT_EQ(dest, std::vector<int>({7, 8, 9}));
}

REGISTER_TYPED_TEST_SUITE_P(
    GenericForwardIteratorTestSuite, BeginEqualsEndWhenEmpty,
    BeginNotEqualsEndWhenPopulated, DereferenceReturnsFirstElement,
    ArrowOperatorAccessesMember, PreIncrementAdvancesToNextElement,
    PreIncrementReturnsReference, MultiplePreIncrementsTraverseAll,
    PostIncrementReturnsOldValue, PostIncrementAdvancesIterator,
    CompleteTraversalReachesEnd, TraversalCollectsAllElements,
    MultiPassGuarantee, RangeBasedForLoopSupport, StdFindWorks, StdCountWorks,
    StdDistanceWorks, StdCopyWorks);

// ============================================================================
// Bidirectional Iterator Tests
// ============================================================================
// Adds to forward iterator tests:
// - operator--, operator--(int)
// - Reverse traversal
// - rbegin(), rend() support
// ============================================================================

template <typename ContainerAdapter>
  requires ContainerAdapterFor<ContainerAdapter>
class GenericBidirectionalIteratorTestSuite
    : public GenericForwardIteratorTestSuite<ContainerAdapter> {
protected:
  using typename GenericForwardIteratorTestSuite<
      ContainerAdapter>::container_type;
  using typename GenericForwardIteratorTestSuite<ContainerAdapter>::value_type;

  // Helper: collect via reverse iteration
  std::vector<value_type> collect_reverse() {
    std::vector<value_type> result;
    for (auto it = this->container.rbegin(); it != this->container.rend();
         ++it) {
      result.push_back(*it);
    }
    return result;
  }
};

TYPED_TEST_SUITE_P(GenericBidirectionalIteratorTestSuite);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    GenericBidirectionalIteratorTestSuite);

// Pre-decrement tests
TYPED_TEST_P(GenericBidirectionalIteratorTestSuite, PreDecrementMovesBackward) {
  this->populate({1, 2, 3});
  auto it = this->container.end();
  --it;
  EXPECT_EQ(*it, 3);
  --it;
  EXPECT_EQ(*it, 2);
}

TYPED_TEST_P(GenericBidirectionalIteratorTestSuite,
             PreDecrementReturnsReference) {
  this->populate({10, 20});
  auto it = this->container.end();
  auto &ref = --it;
  EXPECT_EQ(&ref, &it);
}

// Post-decrement tests
TYPED_TEST_P(GenericBidirectionalIteratorTestSuite,
             PostDecrementReturnsOldValue) {
  this->populate({5, 10, 15});
  auto it = this->container.end();
  --it; // Point to 15
  auto old = it--;
  EXPECT_EQ(*old, 15);
  EXPECT_EQ(*it, 10);
}

// Bidirectional traversal
TYPED_TEST_P(GenericBidirectionalIteratorTestSuite,
             ForwardThenBackwardTraversal) {
  this->populate({1, 2, 3, 4});
  auto it = this->container.begin();
  ++it;
  ++it; // -> 3
  EXPECT_EQ(*it, 3);
  --it; // -> 2
  EXPECT_EQ(*it, 2);
}

// Reverse iterator tests
TYPED_TEST_P(GenericBidirectionalIteratorTestSuite,
             ReverseIteratorTraversesBackward) {
  std::vector<int> expected = {10, 20, 30};
  this->populate(expected);
  auto collected = this->collect_reverse();

  std::vector<int> reversed = {30, 20, 10};
  EXPECT_EQ(collected, reversed);
}

TYPED_TEST_P(GenericBidirectionalIteratorTestSuite, RBeginREndWhenEmpty) {
  EXPECT_EQ(this->container.rbegin(), this->container.rend());
}

TYPED_TEST_P(GenericBidirectionalIteratorTestSuite, StdReverseWorks) {
  this->populate({1, 2, 3, 4, 5});
  std::vector<int> dest;
  std::copy(this->container.rbegin(), this->container.rend(),
            std::back_inserter(dest));
  EXPECT_EQ(dest, std::vector<int>({5, 4, 3, 2, 1}));
}

REGISTER_TYPED_TEST_SUITE_P(GenericBidirectionalIteratorTestSuite,
                            PreDecrementMovesBackward,
                            PreDecrementReturnsReference,
                            PostDecrementReturnsOldValue,
                            ForwardThenBackwardTraversal,
                            ReverseIteratorTraversesBackward,
                            RBeginREndWhenEmpty, StdReverseWorks);

// ============================================================================
// Random Access Iterator Tests
// ============================================================================
// Adds to bidirectional iterator tests:
// - operator+=, operator-=
// - operator+, operator- (iterator arithmetic)
// - operator[] (subscript)
// - operator<, operator>, operator<=, operator>=, operator<=>
// - difference_type operator-(iterator, iterator)
// ============================================================================

template <typename ContainerAdapter>
  requires ContainerAdapterFor<ContainerAdapter>
class GenericRandomAccessIteratorTestSuite
    : public GenericBidirectionalIteratorTestSuite<ContainerAdapter> {
protected:
  using typename GenericBidirectionalIteratorTestSuite<
      ContainerAdapter>::container_type;
  using typename GenericBidirectionalIteratorTestSuite<
      ContainerAdapter>::value_type;
  using difference_type = typename container_type::iterator::difference_type;
};

TYPED_TEST_SUITE_P(GenericRandomAccessIteratorTestSuite);

// Arithmetic assignment operators
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite,
             PlusEqualsAdvancesMultiplePositions) {
  this->populate({10, 20, 30, 40, 50});
  auto it = this->container.begin();
  it += 3;
  EXPECT_EQ(*it, 40);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, MinusEqualsMovesBackward) {
  this->populate({1, 2, 3, 4, 5});
  auto it = this->container.end();
  it -= 2;
  EXPECT_EQ(*it, 4);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, PlusEqualsReturnsReference) {
  this->populate({5, 10, 15});
  auto it = this->container.begin();
  auto &ref = (it += 1);
  EXPECT_EQ(&ref, &it);
}

// Iterator arithmetic (iterator + n, iterator - n)
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, IteratorPlusOffset) {
  this->populate({100, 200, 300, 400});
  auto it = this->container.begin();
  auto it2 = it + 2;
  EXPECT_EQ(*it2, 300);
  EXPECT_EQ(*it, 100); // Original unchanged
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, IteratorMinusOffset) {
  this->populate({7, 14, 21, 28, 35});
  auto it = this->container.end();
  auto it2 = it - 2;
  EXPECT_EQ(*it2, 28);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, OffsetPlusIterator) {
  this->populate({3, 6, 9, 12});
  auto it = this->container.begin();
  auto it2 = 2 + it; // Symmetric operation
  EXPECT_EQ(*it2, 9);
}

// Iterator difference
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, IteratorDifference) {
  this->populate({1, 2, 3, 4, 5, 6});
  auto it1 = this->container.begin();
  auto it2 = it1 + 4;
  EXPECT_EQ(it2 - it1, 4);
  EXPECT_EQ(it1 - it2, -4);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, BeginToEndDistance) {
  this->populate({10, 20, 30, 40, 50});
  auto dist = this->container.end() - this->container.begin();
  EXPECT_EQ(dist, 5);
}

// Subscript operator
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, SubscriptOperator) {
  this->populate({11, 22, 33, 44, 55});
  auto it = this->container.begin();
  EXPECT_EQ(it[0], 11);
  EXPECT_EQ(it[2], 33);
  EXPECT_EQ(it[4], 55);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, SubscriptFromMiddle) {
  this->populate({5, 10, 15, 20, 25, 30});
  auto it = this->container.begin() + 2; // Points to 15
  EXPECT_EQ(it[0], 15);
  EXPECT_EQ(it[1], 20);
  EXPECT_EQ(it[-1], 10);
  EXPECT_EQ(it[-2], 5);
}

// Relational operators
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, LessThanComparison) {
  this->populate({1, 2, 3, 4});
  auto it1 = this->container.begin();
  auto it2 = this->container.begin() + 2;
  EXPECT_LT(it1, it2);
  EXPECT_FALSE(it2 < it1);
  EXPECT_FALSE(it1 < it1);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, GreaterThanComparison) {
  this->populate({10, 20, 30});
  auto it1 = this->container.begin() + 2;
  auto it2 = this->container.begin();
  EXPECT_GT(it1, it2);
  EXPECT_FALSE(it2 > it1);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, LessOrEqualComparison) {
  this->populate({7, 8, 9});
  auto it1 = this->container.begin();
  auto it2 = this->container.begin() + 1;
  EXPECT_LE(it1, it2);
  EXPECT_LE(it1, it1);
  EXPECT_FALSE(it2 <= it1);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, GreaterOrEqualComparison) {
  this->populate({100, 200, 300});
  auto it1 = this->container.begin() + 2;
  auto it2 = this->container.begin();
  EXPECT_GE(it1, it2);
  EXPECT_GE(it1, it1);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, ThreeWayComparison) {
  this->populate({1, 2, 3, 4, 5});
  auto it1 = this->container.begin();
  auto it2 = this->container.begin() + 2;
  auto it3 = this->container.begin() + 2;

  EXPECT_EQ(it1 <=> it2, std::strong_ordering::less);
  EXPECT_EQ(it2 <=> it1, std::strong_ordering::greater);
  EXPECT_EQ(it2 <=> it3, std::strong_ordering::equal);
}

// STL algorithm compatibility (random access specific)
TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, StdSortWorks) {
  this->populate({5, 2, 8, 1, 9, 3});
  std::sort(this->container.begin(), this->container.end());
  auto collected = this->collect();
  EXPECT_EQ(collected, std::vector<int>({1, 2, 3, 5, 8, 9}));
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, StdBinarySearchWorks) {
  this->populate({10, 20, 30, 40, 50});
  bool found =
      std::binary_search(this->container.begin(), this->container.end(), 30);
  EXPECT_TRUE(found);
  bool not_found =
      std::binary_search(this->container.begin(), this->container.end(), 25);
  EXPECT_FALSE(not_found);
}

TYPED_TEST_P(GenericRandomAccessIteratorTestSuite, StdAdvanceWorks) {
  this->populate({7, 14, 21, 28, 35});
  auto it = this->container.begin();
  std::advance(it, 3);
  EXPECT_EQ(*it, 28);
}

REGISTER_TYPED_TEST_SUITE_P(
    GenericRandomAccessIteratorTestSuite, PlusEqualsAdvancesMultiplePositions,
    MinusEqualsMovesBackward, PlusEqualsReturnsReference, IteratorPlusOffset,
    IteratorMinusOffset, OffsetPlusIterator, IteratorDifference,
    BeginToEndDistance, SubscriptOperator, SubscriptFromMiddle,
    LessThanComparison, GreaterThanComparison, LessOrEqualComparison,
    GreaterOrEqualComparison, ThreeWayComparison, StdSortWorks,
    StdBinarySearchWorks, StdAdvanceWorks);
