radiant_plugin(ufoaiplug
        ufoai.cpp ufoai.h
        ufoai_filters.cpp ufoai_filters.h
        ufoai_gtk.cpp ufoai_gtk.h
        ufoai_level.cpp ufoai_level.h
        )

find_package(GTK2 REQUIRED)
target_include_directories(ufoaiplug PRIVATE ${GTK2_INCLUDE_DIRS})
