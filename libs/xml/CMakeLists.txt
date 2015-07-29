find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

add_library(xmllib
    ixml.cpp
    ixml.h
    xmlelement.cpp
    xmlelement.h
    xmlparser.cpp
    xmlparser.h
    xmltextags.cpp
    xmltextags.h
    xmlwriter.cpp
    xmlwriter.h
)
target_link_libraries(xmllib ${LIBXML2_LIBRARIES})
