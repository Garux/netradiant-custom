
radiant_plugin(vfspk3
    archive.cpp
    archive.h
    vfs.cpp
    vfs.h
    vfspk3.cpp
    vfspk3.h
)

find_package(GLIB REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

target_link_libraries(vfspk3 filematch ${GLIB_LIBRARIES})
