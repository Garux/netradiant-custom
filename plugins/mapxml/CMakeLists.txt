
radiant_plugin(mapxml
    plugin.cpp
    plugin.h
    xmlparse.cpp
    xmlparse.h
    xmlwrite.cpp
    xmlwrite.h
)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

target_link_libraries(mapxml ${LIBXML2_LIBRARIES})
