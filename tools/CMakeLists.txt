set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

macro(radiant_tool name)
    add_executable(${name} ${ARGN})
    if (NOT (CMAKE_EXECUTABLE_SUFFIX STREQUAL RADIANT_EXECUTABLE))
        add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ln -f -s "$<TARGET_FILE_NAME:${name}>" "${PROJECT_BINARY_DIR}/${name}.${RADIANT_EXECUTABLE}"
                VERBATIM
                )
    endif ()
endmacro()

add_subdirectory(quake3)
