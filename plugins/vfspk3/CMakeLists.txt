
radiant_plugin(vfspk3
    archive.cpp
    archive.h
    vfs.cpp
    vfs.h
    vfspk3.cpp
    vfspk3.h
)

target_link_libraries(vfspk3 filematch)
