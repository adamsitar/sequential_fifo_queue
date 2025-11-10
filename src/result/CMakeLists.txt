set(LIB_NAME result)
add_library(${LIB_NAME} INTERFACE)

target_include_directories(
  ${LIB_NAME}
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(
  ${LIB_NAME}
  INTERFACE
  $<$<CXX_COMPILER_ID:GNU>:stdc++exp>
  $<$<CXX_COMPILER_ID:Clang>:stdc++exp>
)

