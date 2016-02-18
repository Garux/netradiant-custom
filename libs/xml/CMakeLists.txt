add_library(xmllib
        ixml.h
        xmlelement.h
        xmlparser.h
        xmltextags.cpp xmltextags.h
        xmlwriter.h
        )

find_package(GLIB REQUIRED)
target_include_directories(xmllib PUBLIC ${GLIB_INCLUDE_DIRS})

find_package(LibXml2 REQUIRED)
target_include_directories(xmllib PUBLIC ${LIBXML2_INCLUDE_DIR})
target_link_libraries(xmllib ${LIBXML2_LIBRARIES})
