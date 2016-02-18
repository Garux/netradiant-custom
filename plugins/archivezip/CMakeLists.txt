radiant_plugin(archivezip
        archive.cpp archive.h
        pkzip.h
        plugin.cpp
        zlibstream.h
        )

find_package(ZLIB REQUIRED)
target_include_directories(archivezip PRIVATE ${ZLIB_INCLUDE_DIRS})
