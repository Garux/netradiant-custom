radiant_plugin(mapxml
        plugin.cpp
        xmlparse.cpp xmlparse.h
        xmlwrite.cpp xmlwrite.h
        )

target_link_libraries(mapxml xmllib)
