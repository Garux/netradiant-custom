add_library(xmllib
        ixml.cpp ixml.h
        xmlelement.cpp xmlelement.h
        xmlparser.cpp xmlparser.h
        xmltextags.cpp xmltextags.h
        xmlwriter.cpp xmlwriter.h
        )

find_package(GLIB REQUIRED)
target_include_directories(xmllib PUBLIC ${GLIB_INCLUDE_DIRS})
target_link_libraries(xmllib PUBLIC ${GLIB_LIBRARIES})

find_package(LibXml2 REQUIRED)
target_include_directories(xmllib PUBLIC ${LIBXML2_INCLUDE_DIR})
target_link_libraries(xmllib PUBLIC ${LIBXML2_LIBRARIES})
