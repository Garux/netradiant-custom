radiant_plugin(brushexport
        callbacks.cpp callbacks.h
        export.cpp export.h
        interface.cpp
        plugin.cpp plugin.h
        support.cpp support.h
        )

target_include_directories(brushexport PRIVATE uilib)
target_link_libraries(brushexport PRIVATE uilib)
