set(LIB_NAME queue_c_api)

# Build as a static library (compiled, not INTERFACE)
# This contains the PIMPL implementation that hides C++ templates
add_library(${LIB_NAME} STATIC
  queue_c_api.cpp
)

# Public include directory for C header
target_include_directories(
  ${LIB_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Private include directories for C++ implementation
target_include_directories(
  ${LIB_NAME}
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/allocators
  ${CMAKE_SOURCE_DIR}/src/datastructures
)

# Link against required libraries
target_link_libraries(
  ${LIB_NAME}
  PRIVATE
  core
  allocators
  datastructures
)

# C++ standard library (needed for template instantiation)
target_link_libraries(
  ${LIB_NAME}
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:stdc++exp>
  $<$<CXX_COMPILER_ID:Clang>:stdc++exp>
)
