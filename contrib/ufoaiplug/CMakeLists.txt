
radiant_plugin(ufoaiplug
    ufoai.cpp
    ufoai.h
    ufoai_filters.cpp
    ufoai_filters.h
    ufoai_gtk.cpp
    ufoai_gtk.h
    ufoai_level.cpp
    ufoai_level.h
)

target_link_libraries(ufoaiplug ${GTK2_LIBRARIES})
