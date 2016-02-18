radiant_plugin(prtview
        AboutDialog.cpp AboutDialog.h
        ConfigDialog.cpp ConfigDialog.h
        LoadPortalFileDialog.cpp LoadPortalFileDialog.h
        portals.cpp portals.h
        prtview.cpp prtview.h
        )

find_package(GTK2 REQUIRED)
target_include_directories(prtview PRIVATE ${GTK2_INCLUDE_DIRS})
