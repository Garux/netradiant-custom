# Todo split further and actually compile the tools

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.c.in" "${CMAKE_BINARY_DIR}/version.c")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(quake3)
add_subdirectory(quake2)
