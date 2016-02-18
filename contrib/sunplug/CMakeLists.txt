radiant_plugin(sunplug
        sunplug.cpp sunplug.h
        )

find_package(GTK2 REQUIRED)
target_include_directories(sunplug PRIVATE ${GTK2_INCLUDE_DIRS})
