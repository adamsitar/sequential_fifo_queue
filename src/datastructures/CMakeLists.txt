set(LIB_NAME datastructures)
add_library(${LIB_NAME} INTERFACE)
target_include_directories(
  ${LIB_NAME}
  INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
  ${LIB_NAME}
  INTERFACE
  core
  iterators
  allocators
)


#### test executable
# file(GLOB TEST_FILES CONFIGURE_DEPENDS "*.t.cpp")
add_executable(
  ${LIB_NAME}_test
  "offset_list.t.cpp"
  "ring_buffer.t.cpp"
  "queue.t.cpp"
  "queue_assignment.t.cpp"
  # ${TEST_FILES}
)
find_package(GTest REQUIRED)
target_link_libraries(
  ${LIB_NAME}_test PRIVATE
  ${LIB_NAME}
  allocators
  GTest::gtest_main
)
target_precompile_headers(${LIB_NAME}_test REUSE_FROM pch_base)
gtest_discover_tests(${LIB_NAME}_test)
