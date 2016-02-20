radiant_plugin(sunplug
        sunplug.cpp sunplug.h
        )

target_include_directories(sunplug PRIVATE uilib)
target_link_libraries(sunplug PRIVATE uilib)
