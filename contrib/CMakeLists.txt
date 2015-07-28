# Todo split further and actually compile the plugins

macro(radiant_plugin name)
	message(STATUS "Found Plugin ${name}")
	add_library(${name} MODULE ${ARGN})
endmacro()

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
