#pragma once

#include <gtest/gtest.h>
#include <algorithm>
#include <compare>
#include <cstddef>
#include <new>
#include <vector>

// ============================================================================
// Generic Pointer Test Suite
// ============================================================================
// This header provides generic tests for any pointer type implementing the
// pointer_operations interface (via CRTP). Tests verify pointer arithmetic,
// comparison, dereferencing, and null handling.
//
// Usage:
//   1. Create PointerAdapter for your pointer type
//   2. Add adapter to appropriate type list
//   3. Tests automatically instantiate for your pointer
//
// Adapter Interface:
//   - pointer_type: The smart pointer type
//   - value_type: Element type (e.g., int)
//   - allocator_type: Allocator or allocator bundle
//   - setup_allocator(allocator_type&)
//   - cleanup_allocator(allocator_type&)
//   - allocate_array(allocator, size) -> raw pointer
//   - deallocate_array(allocator, raw_ptr, size)
//   - make_pointer(allocator, raw_ptr) -> pointer_type
//   - to_raw(pointer_type) -> raw pointer
// ============================================================================

// ============================================================================
// Pointer Operations Test
// ============================================================================
// Tests pointer_operations interface: arithmetic, comparison, dereferencing
// ============================================================================

template <typename PointerAdapter>
class PointerOperationsTest : public ::testing::Test {
protected:
  using pointer_type = typename PointerAdapter::pointer_type;
  using value_type = typename PointerAdapter::value_type;
  using allocator_type = typename PointerAdapter::allocator_type;

  allocator_type allocator;
  alignas(value_type*) std::byte array_storage[sizeof(value_type*)];

  value_type*& array() {
    return *std::launder(reinterpret_cast<value_type**>(array_storage));
  }

  static constexpr size_t array_size = 10;

  void SetUp() override {
    PointerAdapter::setup_allocator(allocator);
    // Allocate and initialize test array
    new (&array()) value_type*(PointerAdapter::allocate_array(allocator, array_size));
    for (size_t i = 0; i < array_size; ++i) {
      array()[i] = static_cast<value_type>(i * 10); // [0, 10, 20, 30, ...]
    }
  }

  void TearDown() override {
    PointerAdapter::deallocate_array(allocator, array(), array_size);
    PointerAdapter::cleanup_allocator(allocator);
  }

  pointer_type make_ptr(size_t index = 0) {
    return PointerAdapter::make_pointer(allocator, array() + index);
  }
};

TYPED_TEST_SUITE_P(PointerOperationsTest);

// ============================================================================
// Null Pointer Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, DefaultConstructedIsNull) {
  typename TypeParam::pointer_type ptr;
  EXPECT_EQ(ptr, nullptr);
  EXPECT_FALSE(static_cast<bool>(ptr));
}

TYPED_TEST_P(PointerOperationsTest, NullptrConstructedIsNull) {
  typename TypeParam::pointer_type ptr{nullptr};
  EXPECT_EQ(ptr, nullptr);
  EXPECT_FALSE(ptr);
}

TYPED_TEST_P(PointerOperationsTest, ValidPointerIsNotNull) {
  auto ptr = this->make_ptr(0);
  EXPECT_NE(ptr, nullptr);
  EXPECT_TRUE(ptr);
}

// ============================================================================
// Dereference Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, DereferenceReturnsValue) {
  auto ptr = this->make_ptr(2);
  EXPECT_EQ(*ptr, 20);
}

TYPED_TEST_P(PointerOperationsTest, ArrowOperatorWorks) {
  auto ptr = this->make_ptr(3);
  // For primitive types, just verify we can dereference
  EXPECT_EQ(*ptr, 30);
}

TYPED_TEST_P(PointerOperationsTest, SubscriptOperatorWorks) {
  auto ptr = this->make_ptr(0);
  EXPECT_EQ(ptr[0], 0);
  EXPECT_EQ(ptr[2], 20);
  EXPECT_EQ(ptr[5], 50);
}

// ============================================================================
// Increment/Decrement Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, PreIncrementAdvances) {
  auto ptr = this->make_ptr(1);
  ++ptr;
  EXPECT_EQ(*ptr, 20);
}

TYPED_TEST_P(PointerOperationsTest, PreIncrementReturnsReference) {
  auto ptr = this->make_ptr(0);
  auto& ref = ++ptr;
  EXPECT_EQ(&ref, &ptr);
}

TYPED_TEST_P(PointerOperationsTest, PostIncrementReturnsOldValue) {
  auto ptr = this->make_ptr(1);
  auto old = ptr++;
  EXPECT_EQ(*old, 10);
  EXPECT_EQ(*ptr, 20);
}

TYPED_TEST_P(PointerOperationsTest, PreDecrementMovesBackward) {
  auto ptr = this->make_ptr(3);
  --ptr;
  EXPECT_EQ(*ptr, 20);
}

TYPED_TEST_P(PointerOperationsTest, PostDecrementReturnsOldValue) {
  auto ptr = this->make_ptr(3);
  auto old = ptr--;
  EXPECT_EQ(*old, 30);
  EXPECT_EQ(*ptr, 20);
}

// ============================================================================
// Arithmetic Assignment Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, PlusEqualsAdvances) {
  auto ptr = this->make_ptr(0);
  ptr += 3;
  EXPECT_EQ(*ptr, 30);
}

TYPED_TEST_P(PointerOperationsTest, MinusEqualsMovesBackward) {
  auto ptr = this->make_ptr(5);
  ptr -= 2;
  EXPECT_EQ(*ptr, 30);
}

TYPED_TEST_P(PointerOperationsTest, PlusEqualsReturnsReference) {
  auto ptr = this->make_ptr(0);
  auto& ref = (ptr += 1);
  EXPECT_EQ(&ref, &ptr);
}

// ============================================================================
// Arithmetic Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, PointerPlusOffset) {
  auto ptr = this->make_ptr(1);
  auto ptr2 = ptr + 2;
  EXPECT_EQ(*ptr2, 30);
  EXPECT_EQ(*ptr, 10); // Original unchanged
}

TYPED_TEST_P(PointerOperationsTest, OffsetPlusPointer) {
  auto ptr = this->make_ptr(1);
  auto ptr2 = 2 + ptr; // Symmetric operation
  EXPECT_EQ(*ptr2, 30);
}

TYPED_TEST_P(PointerOperationsTest, PointerMinusOffset) {
  auto ptr = this->make_ptr(5);
  auto ptr2 = ptr - 2;
  EXPECT_EQ(*ptr2, 30);
}

TYPED_TEST_P(PointerOperationsTest, PointerDifference) {
  auto ptr1 = this->make_ptr(2);
  auto ptr2 = this->make_ptr(6);
  EXPECT_EQ(ptr2 - ptr1, 4);
  EXPECT_EQ(ptr1 - ptr2, -4);
}

// ============================================================================
// Comparison Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, EqualityComparison) {
  auto ptr1 = this->make_ptr(3);
  auto ptr2 = this->make_ptr(3);
  auto ptr3 = this->make_ptr(4);

  EXPECT_EQ(ptr1, ptr2);
  EXPECT_NE(ptr1, ptr3);
}

TYPED_TEST_P(PointerOperationsTest, LessThanComparison) {
  auto ptr1 = this->make_ptr(2);
  auto ptr2 = this->make_ptr(5);

  EXPECT_LT(ptr1, ptr2);
  EXPECT_FALSE(ptr2 < ptr1);
  EXPECT_FALSE(ptr1 < ptr1);
}

TYPED_TEST_P(PointerOperationsTest, GreaterThanComparison) {
  auto ptr1 = this->make_ptr(7);
  auto ptr2 = this->make_ptr(3);

  EXPECT_GT(ptr1, ptr2);
  EXPECT_FALSE(ptr2 > ptr1);
}

TYPED_TEST_P(PointerOperationsTest, LessOrEqualComparison) {
  auto ptr1 = this->make_ptr(3);
  auto ptr2 = this->make_ptr(5);
  auto ptr3 = this->make_ptr(3);

  EXPECT_LE(ptr1, ptr2);
  EXPECT_LE(ptr1, ptr3);
  EXPECT_FALSE(ptr2 <= ptr1);
}

TYPED_TEST_P(PointerOperationsTest, GreaterOrEqualComparison) {
  auto ptr1 = this->make_ptr(7);
  auto ptr2 = this->make_ptr(3);
  auto ptr3 = this->make_ptr(7);

  EXPECT_GE(ptr1, ptr2);
  EXPECT_GE(ptr1, ptr3);
}

TYPED_TEST_P(PointerOperationsTest, ThreeWayComparison) {
  auto ptr1 = this->make_ptr(2);
  auto ptr2 = this->make_ptr(5);
  auto ptr3 = this->make_ptr(2);

  EXPECT_EQ(ptr1 <=> ptr2, std::strong_ordering::less);
  EXPECT_EQ(ptr2 <=> ptr1, std::strong_ordering::greater);
  EXPECT_EQ(ptr1 <=> ptr3, std::strong_ordering::equal);
}

// ============================================================================
// Conversion Tests
// ============================================================================

TYPED_TEST_P(PointerOperationsTest, ConversionToRawPointer) {
  auto ptr = this->make_ptr(4);
  auto raw = TypeParam::to_raw(ptr);
  EXPECT_EQ(*raw, 40);
}

TYPED_TEST_P(PointerOperationsTest, ConversionToVoidPointer) {
  auto ptr = this->make_ptr(2);
  void* vptr = static_cast<void*>(ptr);
  EXPECT_NE(vptr, nullptr);
}

REGISTER_TYPED_TEST_SUITE_P(
    PointerOperationsTest,
    DefaultConstructedIsNull,
    NullptrConstructedIsNull,
    ValidPointerIsNotNull,
    DereferenceReturnsValue,
    ArrowOperatorWorks,
    SubscriptOperatorWorks,
    PreIncrementAdvances,
    PreIncrementReturnsReference,
    PostIncrementReturnsOldValue,
    PreDecrementMovesBackward,
    PostDecrementReturnsOldValue,
    PlusEqualsAdvances,
    MinusEqualsMovesBackward,
    PlusEqualsReturnsReference,
    PointerPlusOffset,
    OffsetPlusPointer,
    PointerMinusOffset,
    PointerDifference,
    EqualityComparison,
    LessThanComparison,
    GreaterThanComparison,
    LessOrEqualComparison,
    GreaterOrEqualComparison,
    ThreeWayComparison,
    ConversionToRawPointer,
    ConversionToVoidPointer
);
