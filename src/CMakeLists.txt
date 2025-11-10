# Add subdirectories - result and iterators have no dependencies
# pointers depends on result
# core depends on result and iterators
# pch is built first to create precompiled headers for reuse
add_subdirectory(pch)
add_subdirectory(result)
add_subdirectory(iterators)
add_subdirectory(pointers)
add_subdirectory(core)
add_subdirectory(allocators)
add_subdirectory(datastructures)
add_subdirectory(c_api)

# Main executable
add_executable(main main.cpp)

# Include types.h from src directory
target_include_directories(
  main
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against both libraries
target_link_libraries(
  main
  PRIVATE
  allocators
  datastructures
)

# Reuse precompiled headers from pch_base
target_precompile_headers(main REUSE_FROM pch_base)
