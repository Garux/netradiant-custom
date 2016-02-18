radiant_plugin(shaders
        plugin.cpp
        shaders.cpp shaders.h
        )

find_package(GLIB REQUIRED)
target_include_directories(shaders PRIVATE ${GLIB_INCLUDE_DIRS})
