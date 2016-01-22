
radiant_plugin(archivezip
    archive.cpp
    archive.h
    pkzip.cpp
    pkzip.h
    plugin.cpp
    plugin.h
    zlibstream.cpp
    zlibstream.h
)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

target_link_libraries(archivezip ${ZLIB_LIBRARIES})
