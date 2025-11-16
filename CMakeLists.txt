cmake_minimum_required(VERSION 4.1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(queue CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)


# Precompiled Headers for iterator library
option(ITERATORS_USE_PCH "Enable precompiled headers for iterators library" OFF)

add_definitions(-w)

# hardening
add_compile_options(
  # Warnings (commented out - uncomment to enable)
  # -Wall -Wextra -Wpedantic
  # -Wformat=2 -Wformat-security -Wnull-dereference
  # -Wconversion -Wsign-conversion -Wcast-align
  # -Werror=format-security -Werror=return-type
  -Wuninitialized
  -fPIE # for ASLR
  $<$<CONFIG:Debug>:-ftrivial-auto-var-init=pattern>
  $<$<CONFIG:Release>:-fstack-protector-strong>
  $<$<CONFIG:Release>:-fno-strict-overflow>
  $<$<CONFIG:Release>:-ftrivial-auto-var-init=zero>
  $<$<CONFIG:Release>:-fcf-protection=full>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-fstack-clash-protection>

  # Build profiling - generates .json trace files
  # -ftime-trace
)
add_link_options(-pie)

add_compile_definitions(
  _GLIBCXX_ASSERTIONS                      # STL bounds checking (all builds)
  $<$<CONFIG:Release>:_FORTIFY_SOURCE=3>   # Buffer overflow detection (Release only)
)

include(Sanitize)

enable_testing()
add_subdirectory(src)

# Custom target to build main test suites
add_custom_target(all_tests
  DEPENDS
    allocators_test
    datastructures_test
    c_api_test
)
