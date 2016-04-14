add_library(os
        dir.cpp dir.h
        file.cpp file.h
        path.cpp path.h
        )

find_package(GLIB REQUIRED)
target_include_directories(os PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_libraries(os PRIVATE ${GLIB_LIBRARIES})
