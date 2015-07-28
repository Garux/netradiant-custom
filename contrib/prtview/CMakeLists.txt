
radiant_plugin(prtview
    AboutDialog.cpp
    AboutDialog.h
    ConfigDialog.cpp
    ConfigDialog.h
    LoadPortalFileDialog.cpp
    LoadPortalFileDialog.h
    portals.cpp
    portals.h
    prtview.cpp
    prtview.h
)

target_link_libraries(prtview profile)
