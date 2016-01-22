
radiant_plugin(shaderplug
    shaderplug.cpp
    shaderplug.h
)

target_link_libraries(shaderplug xmllib ${GTK2_LIBRARIES})
