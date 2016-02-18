radiant_plugin(vfspk3
        archive.cpp archive.h
        vfs.cpp vfs.h
        vfspk3.cpp
        )

find_package(GLIB REQUIRED)
target_include_directories(vfspk3 PRIVATE ${GLIB_INCLUDE_DIRS})

target_link_libraries(vfspk3 filematch)
