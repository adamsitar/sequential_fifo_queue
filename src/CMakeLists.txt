# Add subdirectories - core must come first (provides shared types/concepts)
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
