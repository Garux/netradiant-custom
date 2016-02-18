add_library(os
        dir.h
        file.h
        path.h
        )
set_target_properties(os PROPERTIES LINKER_LANGUAGE CXX)

find_package(GLIB REQUIRED)
target_include_directories(os PRIVATE ${GLIB_INCLUDE_DIRS})

