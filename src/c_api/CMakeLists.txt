set(LIB_NAME queue_c_api)

add_library(${LIB_NAME} STATIC
  queue_c_api.cpp
)

target_include_directories(
  ${LIB_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(
  ${LIB_NAME}
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/allocators
  ${CMAKE_SOURCE_DIR}/src/datastructures
)

target_link_libraries(
  ${LIB_NAME}
  PRIVATE
  core
  allocators
  datastructures
)

target_link_libraries(
  ${LIB_NAME}
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:stdc++exp>
  $<$<CXX_COMPILER_ID:Clang>:stdc++exp>
)
