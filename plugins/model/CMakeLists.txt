radiant_plugin(model
        model.cpp model.h
        plugin.cpp
        )

target_include_directories(model PRIVATE picomodel)
target_link_libraries(model PRIVATE picomodel)
