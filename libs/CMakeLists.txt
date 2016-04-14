add_subdirectory(cmdlib)
add_subdirectory(container)
add_subdirectory(ddslib)
add_subdirectory(debugging)
add_subdirectory(etclib)
add_subdirectory(filematch)
add_subdirectory(generic)
if (BUILD_RADIANT)
    add_subdirectory(gtkutil)
endif ()
add_subdirectory(l_net)
add_subdirectory(math)
add_subdirectory(mathlib)
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
        archivelib.cpp archivelib.h
        bytebool.cpp bytebool.h
        bytestreamutils.cpp bytestreamutils.h
        character.cpp character.h
        convert.cpp convert.h
        dragplanes.cpp dragplanes.h
        eclasslib.cpp eclasslib.h
        entitylib.cpp entitylib.h
        entityxml.cpp entityxml.h
        fs_filesystem.cpp fs_filesystem.h
        fs_path.cpp fs_path.h
        imagelib.cpp imagelib.h
        instancelib.cpp instancelib.h
        maplib.cpp maplib.h
        moduleobservers.cpp moduleobservers.h
        pivot.cpp pivot.h
        render.cpp render.h
        scenelib.cpp scenelib.h
        selectionlib.cpp selectionlib.h
        shaderlib.cpp shaderlib.h
        str.cpp str.h
        stringio.cpp stringio.h
        texturelib.cpp texturelib.h
        transformlib.cpp transformlib.h
        traverselib.cpp traverselib.h
        typesystem.cpp typesystem.h
        undolib.cpp undolib.h
        uniquenames.cpp uniquenames.h
        versionlib.cpp versionlib.h
        )

find_package(GLIB REQUIRED)
target_include_directories(libs PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_libraries(libs PRIVATE ${GLIB_LIBRARIES})
