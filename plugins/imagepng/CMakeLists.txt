radiant_plugin(imagepng
    plugin.cpp
    plugin.h
)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

target_link_libraries(imagepng ${PNG_LIBRARIES})
