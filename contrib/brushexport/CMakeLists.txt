radiant_plugin(brushexport
        callbacks.cpp callbacks.h
        export.cpp export.h
        interface.cpp
        plugin.cpp plugin.h
        support.cpp support.h
        )

find_package(GTK2 REQUIRED)
target_include_directories(brushexport PRIVATE ${GTK2_INCLUDE_DIRS})
