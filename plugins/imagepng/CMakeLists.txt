radiant_plugin(imagepng
        plugin.cpp
        )

find_package(PNG REQUIRED)
target_include_directories(imagepng PRIVATE ${PNG_INCLUDE_DIR})
target_link_libraries(imagepng PRIVATE ${PNG_LIBRARIES})
