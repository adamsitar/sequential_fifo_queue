add_library(pch_base OBJECT pch_dummy.cpp)
target_compile_features(pch_base PUBLIC cxx_std_23)
target_precompile_headers(
  pch_base
  PUBLIC
  <memory>
  <memory_resource>
  <new>
  <type_traits>
  <concepts>
  <utility>
  <iterator>
  <compare>
  <cstddef>
  <cstdint>
  <bit>
)
