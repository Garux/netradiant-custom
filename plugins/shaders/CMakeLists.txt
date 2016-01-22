
radiant_plugin(shaders
    plugin.cpp
    plugin.h
    shaders.cpp
    shaders.h
)

find_package(GLIB REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

target_link_libraries(shaders ${GLIB_LIBRARIES})
