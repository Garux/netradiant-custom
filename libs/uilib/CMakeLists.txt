add_library(uilib
        uilib.cpp
        )

find_package(GTK2 REQUIRED)
target_include_directories(uilib PUBLIC ${${GTK_NS}_INCLUDE_DIRS})
target_link_libraries(uilib PUBLIC ${${GTK_NS}_LIBRARIES})

target_include_directories(uilib PUBLIC gtkutil)
target_link_libraries(uilib PUBLIC gtkutil)
