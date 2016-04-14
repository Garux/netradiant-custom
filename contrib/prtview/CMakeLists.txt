radiant_plugin(prtview
        AboutDialog.cpp AboutDialog.h
        ConfigDialog.cpp ConfigDialog.h
        LoadPortalFileDialog.cpp LoadPortalFileDialog.h
        portals.cpp portals.h
        prtview.cpp prtview.h
        )

target_include_directories(prtview PRIVATE uilib)
target_link_libraries(prtview PRIVATE uilib)

target_include_directories(prtview PRIVATE profile)
target_link_libraries(prtview PRIVATE profile)
