include_directories("${CMAKE_SOURCE_DIR}/libs")
include_directories("${CMAKE_SOURCE_DIR}/include")

find_package(GTK2 REQUIRED)
include_directories(${GTK2_INCLUDE_DIRS} ${GTK2_GDK_INCLUDE_DIR} ${GTK2_GLIB_INCLUDE_DIR})

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")

# add_subdirectory(bkgrnd2d)
add_subdirectory(bobtoolz)
add_subdirectory(brushexport)
# add_subdirectory(camera)
# add_subdirectory(gtkgensurf)
# add_subdirectory(hydratoolz)
add_subdirectory(prtview)
add_subdirectory(shaderplug)
add_subdirectory(sunplug)
add_subdirectory(ufoaiplug)
