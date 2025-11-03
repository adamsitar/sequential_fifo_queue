set(LIB_NAME allocators)

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
)

# test executable
add_executable(
  ${LIB_NAME}_test
  "local_buffer.t.cpp"
  "freelist.t.cpp"
  "dynamic_buffer.t.cpp"
  # ${TEST_FILES}
)

find_package(GTest REQUIRED)
target_link_libraries(
  ${LIB_NAME}_test PRIVATE
  ${LIB_NAME}
  GTest::gtest_main
)

gtest_discover_tests(${LIB_NAME}_test)
