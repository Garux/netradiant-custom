set(CMAKE_POSITION_INDEPENDENT_CODE 1)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(cmdlib)
add_subdirectory(container)
add_subdirectory(ddslib)
add_subdirectory(debugging)
add_subdirectory(etclib)
add_subdirectory(filematch)
add_subdirectory(generic)
if (NOT SKIP_RADIANT)
    add_subdirectory(gtkutil)
endif ()
add_subdirectory(l_net)
add_subdirectory(math)
add_subdirectory(mathlib)
add_subdirectory(md5lib)
add_subdirectory(memory)
add_subdirectory(modulesystem)
add_subdirectory(os)
add_subdirectory(picomodel)
add_subdirectory(profile)
add_subdirectory(script)
add_subdirectory(signal)
add_subdirectory(splines)
add_subdirectory(stream)
add_subdirectory(string)
add_subdirectory(uilib)
add_subdirectory(xml)

add_library(libs
        _.cpp
        archivelib.h
        bytebool.h
        bytestreamutils.h
        character.h
        convert.h
        dragplanes.h
        eclasslib.h
        entitylib.h
        entityxml.h
        fs_filesystem.h
        fs_path.h
        imagelib.h
        instancelib.h
        maplib.h
        moduleobservers.h
        pivot.h
        render.h
        scenelib.h
        selectionlib.h
        shaderlib.h
        str.h
        stringio.h
        texturelib.h
        transformlib.h
        traverselib.h
        typesystem.h
        undolib.h
        uniquenames.h
        versionlib.h
        )

find_package(GLIB REQUIRED)
target_include_directories(libs PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_libraries(libs PRIVATE ${GLIB_LIBRARIES})
