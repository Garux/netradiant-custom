cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(NetRadiant C CXX)

#CMake 3.0+ would allow this in project()
set(NetRadiant_VERSION_MAJOR 1)
set(NetRadiant_VERSION_MINOR 5)
set(NetRadiant_VERSION_PATCH 0)
set(NetRadiant_VERSION ${NetRadiant_VERSION_MAJOR}.${NetRadiant_VERSION_MINOR}.${NetRadiant_VERSION_PATCH})

set(Q3MAP_VERSION = 2.5.17n)

set(RADIANT_VERSION_STRING "${NetRadiant_VERSION}n${GIT_VERSION}")
execute_process(COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(GIT_VERSION)
    set(RADIANT_VERSION_STRING "${RADIANT_VERSION_STRING}-git-${GIT_VERSION}")
    set(Q3MAP_VERSION "${Q3MAP_VERSION}-git-${GIT_VERSION}")
endif()

if(WIN32)
    set(RADIANT_EXECUTABLE "exe")
else()
    execute_process(COMMAND uname -m
        OUTPUT_VARIABLE RADIANT_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")

message(STATUS "Building ${PROJECT_NAME} ${RADIANT_VERSION_STRING} ${RADIANT_ABOUTMSG}")


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# For some reason the above flags don't really work...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
    if(STD_CXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
    else()
        message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
    endif()
else()
    message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# Enable Debug by default, can be changed with -D CMAKE_BUILD_TYPE=Release
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE Debug)
    add_definitions(-D_DEBUG=1)
else()
endif()

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

find_package(GTK2 REQUIRED)
include_directories(${GTK2_INCLUDE_DIRS} ${GTK2_GDK_INCLUDE_DIR} ${GTK2_GLIB_INCLUDE_DIR})

find_package(GtkGLExt REQUIRED)
include_directories(${GTKGL_INCLUDE_DIRS})

find_package(OpenGL REQUIRED)

add_definitions(
    -DRADIANT_VERSION="${RADIANT_VERSION_STRING}"
    -DRADIANT_MAJOR_VERSION="${NetRadiant_VERSION_MINOR}"
    -DRADIANT_MINOR_VERSION="${NetRadiant_VERSION_PATCH}"
    -DRADIANT_ABOUTMSG="${RADIANT_ABOUTMSG}"
    -DRADIANT_EXECUTABLE="${RADIANT_EXECUTABLE}"
)
if(NOT WIN32)
    add_definitions(
        -DPOSIX=1
        -DXWINDOWS=1
    )
else()
    add_definitions(
        -D_WIN32
    )
endif()

set(INCLUDELIST
    include/aboutmsg.h
    include/cullable.cpp
    include/cullable.h
    include/editable.cpp
    include/editable.h
    include/iarchive.cpp
    include/iarchive.h
    include/ibrush.cpp
    include/ibrush.h
    include/icamera.cpp
    include/icamera.h
    include/idatastream.cpp
    include/idatastream.h
    include/ieclass.cpp
    include/ieclass.h
    include/ientity.cpp
    include/ientity.h
    include/ifilesystem.cpp
    include/ifilesystem.h
    include/ifiletypes.cpp
    include/ifiletypes.h
    include/ifilter.cpp
    include/ifilter.h
    include/igl.cpp
    include/igl.h
    include/iglrender.cpp
    include/iglrender.h
    include/igtkgl.cpp
    include/igtkgl.h
    include/iimage.cpp
    include/iimage.h
    include/imap.cpp
    include/imap.h
    include/imodel.cpp
    include/imodel.h
    include/ipatch.cpp
    include/ipatch.h
    include/iplugin.cpp
    include/iplugin.h
    include/ireference.cpp
    include/ireference.h
    include/irender.cpp
    include/irender.h
    include/iscenegraph.cpp
    include/iscenegraph.h
    include/iscriplib.cpp
    include/iscriplib.h
    include/iselection.cpp
    include/iselection.h
    include/ishaders.cpp
    include/ishaders.h
    include/itexdef.cpp
    include/itexdef.h
    include/itextstream.cpp
    include/itextstream.h
    include/itextures.cpp
    include/itextures.h
    include/itoolbar.cpp
    include/itoolbar.h
    include/iundo.cpp
    include/iundo.h
    include/mapfile.cpp
    include/mapfile.h
    include/modelskin.cpp
    include/modelskin.h
    include/moduleobserver.cpp
    include/moduleobserver.h
    include/modulesystem.cpp
    include/modulesystem.h
    include/nameable.cpp
    include/nameable.h
    include/namespace.cpp
    include/namespace.h
    include/preferencesystem.cpp
    include/preferencesystem.h
    include/qerplugin.cpp
    include/qerplugin.h
    include/renderable.cpp
    include/renderable.h
    include/selectable.cpp
    include/selectable.h
    include/stream_version.h
    include/version.h
    include/warnings.h
    include/windowobserver.cpp
    include/windowobserver.h
)
include_directories(include)

add_subdirectory(libs)
include_directories(libs)

macro(radiant_plugin name)
	message(STATUS "Found Plugin ${name}")
	add_library(${name} MODULE ${ARGN})
endmacro()

add_subdirectory(contrib)
add_subdirectory(plugins)

set(RADIANTLIST
    radiant/autosave.cpp
    radiant/autosave.h
    radiant/brush.cpp
    radiant/brush.h
    radiant/brush_primit.cpp
    radiant/brush_primit.h
    radiant/brushmanip.cpp
    radiant/brushmanip.h
    radiant/brushmodule.cpp
    radiant/brushmodule.h
    radiant/brushnode.cpp
    radiant/brushnode.h
    radiant/brushtokens.cpp
    radiant/brushtokens.h
    radiant/brushxml.cpp
    radiant/brushxml.h
    radiant/build.cpp
    radiant/build.h
    radiant/camwindow.cpp
    radiant/camwindow.h
    radiant/clippertool.cpp
    radiant/clippertool.h
    radiant/commands.cpp
    radiant/commands.h
    radiant/console.cpp
    radiant/console.h
    radiant/csg.cpp
    radiant/csg.h
    radiant/dialog.cpp
    radiant/dialog.h
    radiant/eclass.cpp
    radiant/eclass.h
    radiant/eclass_def.cpp
    radiant/eclass_def.h
    radiant/eclass_doom3.cpp
    radiant/eclass_doom3.h
    radiant/eclass_fgd.cpp
    radiant/eclass_fgd.h
    radiant/eclass_xml.cpp
    radiant/eclass_xml.h
    radiant/entity.cpp
    radiant/entity.h
    radiant/entityinspector.cpp
    radiant/entityinspector.h
    radiant/entitylist.cpp
    radiant/entitylist.h
    radiant/environment.cpp
    radiant/environment.h
    radiant/error.cpp
    radiant/error.h
    radiant/feedback.cpp
    radiant/feedback.h
    radiant/filetypes.cpp
    radiant/filetypes.h
    radiant/filters.cpp
    radiant/filters.h
    radiant/findtexturedialog.cpp
    radiant/findtexturedialog.h
    radiant/glwidget.cpp
    radiant/glwidget.h
    radiant/grid.cpp
    radiant/grid.h
    radiant/groupdialog.cpp
    radiant/groupdialog.h
    radiant/gtkdlgs.cpp
    radiant/gtkdlgs.h
    radiant/gtkmisc.cpp
    radiant/gtkmisc.h
    radiant/help.cpp
    radiant/help.h
    radiant/image.cpp
    radiant/image.h
    radiant/main.cpp
    radiant/main.h
    radiant/mainframe.cpp
    radiant/mainframe.h
    radiant/map.cpp
    radiant/map.h
    radiant/mru.cpp
    radiant/mru.h
    radiant/multimon.cpp
    radiant/multimon.h
    radiant/nullmodel.cpp
    radiant/nullmodel.h
    radiant/parse.cpp
    radiant/parse.h
    radiant/patch.cpp
    radiant/patch.h
    radiant/patchdialog.cpp
    radiant/patchdialog.h
    radiant/patchmanip.cpp
    radiant/patchmanip.h
    radiant/patchmodule.cpp
    radiant/patchmodule.h
    radiant/plugin.cpp
    radiant/plugin.h
    radiant/pluginapi.cpp
    radiant/pluginapi.h
    radiant/pluginmanager.cpp
    radiant/pluginmanager.h
    radiant/pluginmenu.cpp
    radiant/pluginmenu.h
    radiant/plugintoolbar.cpp
    radiant/plugintoolbar.h
    radiant/points.cpp
    radiant/points.h
    radiant/preferencedictionary.cpp
    radiant/preferencedictionary.h
    radiant/preferences.cpp
    radiant/preferences.h
    radiant/qe3.cpp
    radiant/qe3.h
    radiant/qgl.cpp
    radiant/qgl.h
    radiant/referencecache.cpp
    radiant/referencecache.h
    radiant/renderer.cpp
    radiant/renderer.h
    radiant/renderstate.cpp
    radiant/renderstate.h
    radiant/resource.h
    radiant/scenegraph.cpp
    radiant/scenegraph.h
    radiant/select.cpp
    radiant/select.h
    radiant/selection.cpp
    radiant/selection.h
    radiant/server.cpp
    radiant/server.h
    radiant/shaders.cpp
    radiant/shaders.h
    radiant/sockets.cpp
    radiant/sockets.h
    radiant/stacktrace.cpp
    radiant/stacktrace.h
    radiant/surfacedialog.cpp
    radiant/surfacedialog.h
    radiant/texmanip.cpp
    radiant/texmanip.h
    radiant/textureentry.cpp
    radiant/textureentry.h
    radiant/textures.cpp
    radiant/textures.h
    radiant/texwindow.cpp
    radiant/texwindow.h
    radiant/timer.cpp
    radiant/timer.h
    radiant/treemodel.cpp
    radiant/treemodel.h
    radiant/undo.cpp
    radiant/undo.h
    radiant/url.cpp
    radiant/url.h
    radiant/view.cpp
    radiant/view.h
    radiant/watchbsp.cpp
    radiant/watchbsp.h
    radiant/winding.cpp
    radiant/winding.h
    radiant/windowobservers.cpp
    radiant/windowobservers.h
    radiant/xmlstuff.cpp
    radiant/xmlstuff.h
    radiant/xywindow.cpp
    radiant/xywindow.h
)

add_executable(radiant WIN32 radiant/radiant.rc ${RADIANTLIST})
target_link_libraries(radiant
    ${CMAKE_DL_LIBS}
    ${LIBXML2_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${OPENGL_gl_LIBRARY}
    cmdlib
    container
    ddslib
    debugging
    gtkutil
    ${GTKGL_LIBRARIES}
    l_net
#    math
    mathlib
    md5lib
    memory
    modulesystem
    os
    picomodel
    profile
    script
    signal
    splines
    stream
    string
    xmllib
)

if(NOT (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR))
    # Copy data files from sources to the build directory
    message(STATUS "Copying data files")
    file(GLOB DATA_FILES "${CMAKE_SOURCE_DIR}/setup/data/tools/*")
    file(COPY ${DATA_FILES} DESTINATION "${CMAKE_BINARY_DIR}")
    file(GLOB DATA_FILES "${CMAKE_SOURCE_DIR}/docs/*")
    file(COPY ${DATA_FILES} DESTINATION "${CMAKE_BINARY_DIR}/docs")
endif()

option(DOWNLOAD_GAMEPACKS "Download game packs" ON)
if(DOWNLOAD_GAMEPACKS)
    add_custom_target(game_packs ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory games
        COMMAND DOWNLOAD_GAMEPACKS=yes SOURCE_DIR="${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/install-gamepacks.sh" "${CMAKE_BINARY_DIR}"
    )
endif()
