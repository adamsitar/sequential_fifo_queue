# file(GLOB TEST_FILES CONFIGURE_DEPENDS "*.t.cpp")

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
  allocators
)

# test executable
add_executable(
  ${LIB_NAME}_test
  "offset_list.t.cpp"
  # "ring_buffer.t.cpp"
  # "queue.t.cpp"
  # ${TEST_FILES}
)

find_package(GTest REQUIRED)
target_link_libraries(
  ${LIB_NAME}_test PRIVATE
  ${LIB_NAME}
  allocators  # Tests need actual allocator implementations
  GTest::gtest_main
)

gtest_discover_tests(${LIB_NAME}_test)
