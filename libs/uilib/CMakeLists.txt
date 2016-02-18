add_library(uilib
        uilib.cpp uilib.h
        )

find_package(GTK2 REQUIRED)
target_include_directories(uilib PRIVATE ${GTK2_INCLUDE_DIRS})
