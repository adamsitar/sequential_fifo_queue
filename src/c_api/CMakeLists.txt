set(LIB_NAME c_api)
file(GLOB TEST_FILES CONFIGURE_DEPENDS "*.t.cpp")

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
  result
)

target_precompile_headers(${LIB_NAME} REUSE_FROM pch_base)

#### test executable
add_executable(
  ${LIB_NAME}_test
  ${TEST_FILES}
)
find_package(GTest REQUIRED)
target_link_libraries(
  ${LIB_NAME}_test PRIVATE
  ${LIB_NAME}
  GTest::gtest_main
)
target_precompile_headers(${LIB_NAME}_test REUSE_FROM pch_base)
gtest_discover_tests(${LIB_NAME}_test)
