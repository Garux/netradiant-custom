add_library(os
        _.cpp
        dir.h
        file.h
        path.h
        )

find_package(GLIB REQUIRED)
target_include_directories(os PRIVATE ${GLIB_INCLUDE_DIRS})
