file(GLOB SOURCE_FILES CONFIGURE_DEPENDS "*.cpp")
file(GLOB TEST_FILES CONFIGURE_DEPENDS "*.t.cpp")
file(GLOB PRIVATE_HEADERS CONFIGURE_DEPENDS "*.h")

set(LIB_NAME queue)

add_library(${LIB_NAME} STATIC ${SOURCE_FILES})

target_include_directories(
  ${LIB_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# test executable
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

gtest_discover_tests(${LIB_NAME}_test)
