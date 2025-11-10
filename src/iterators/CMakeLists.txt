add_library(iterators INTERFACE)

target_compile_features(
  iterators
  INTERFACE
  cxx_std_23
)

target_include_directories(
  iterators
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
