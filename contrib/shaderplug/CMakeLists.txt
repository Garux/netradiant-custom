radiant_plugin(shaderplug
        shaderplug.cpp shaderplug.h
        )

target_include_directories(shaderplug PRIVATE xmllib)
target_link_libraries(shaderplug PRIVATE xmllib)

target_include_directories(shaderplug PRIVATE uilib)
target_link_libraries(shaderplug PRIVATE uilib)
