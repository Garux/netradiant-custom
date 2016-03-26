cmake_minimum_required(VERSION 2.8.11 FATAL_ERROR)

option(SKIP_RADIANT "don't build the gui, useful for map compile servers" OFF)

project(NetRadiant C CXX)

# Enable Debug by default, can be changed with -DCMAKE_BUILD_TYPE=Release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

#-----------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------

#CMake 3.0+ would allow this in project()
set(NetRadiant_VERSION_MAJOR 1)
set(NetRadiant_VERSION_MINOR 5)
set(NetRadiant_VERSION_PATCH 0)
set(NetRadiant_VERSION ${NetRadiant_VERSION_MAJOR}.${NetRadiant_VERSION_MINOR}.${NetRadiant_VERSION_PATCH})
set(Q3MAP_VERSION 2.5.17n)
set(Q3MAP_STREAM_VERSION 1)

find_package(Git REQUIRED)
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(RADIANT_VERSION_STRING "${NetRadiant_VERSION}n")
if (GIT_VERSION)
    set(RADIANT_VERSION_STRING "${RADIANT_VERSION_STRING}-git-${GIT_VERSION}")
    set(Q3MAP_VERSION "${Q3MAP_VERSION}-git-${GIT_VERSION}")
endif ()

configure_file("${PROJECT_SOURCE_DIR}/include/version.cpp.in" "${PROJECT_BINARY_DIR}/version.cpp")

add_library(version "${PROJECT_BINARY_DIR}/version.cpp")
set_property(TARGET version PROPERTY POSITION_INDEPENDENT_CODE TRUE)

set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")
message(STATUS "Building ${PROJECT_NAME} ${RADIANT_VERSION_STRING} ${RADIANT_ABOUTMSG}")

#-----------------------------------------------------------------------
# Language standard
#-----------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# For some reason the above flags don't really work...
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++${CMAKE_CXX_STANDARD} STD_CXX)
    if (STD_CXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++${CMAKE_CXX_STANDARD}")
    else ()
        message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
    endif ()
else ()
    message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
endif ()

#-----------------------------------------------------------------------
# Defs
#-----------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D_DEBUG=1)
endif ()

add_definitions(
        -DGTK_DISABLE_SINGLE_INCLUDES
        -DGSEAL_ENABLE
        -DGDK_DISABLE_DEPRECATED
        -DGTK_DISABLE_DEPRECATED
)

if (NOT WIN32)
    add_definitions(
            -DPOSIX=1
            -DXWINDOWS=1
    )
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
else ()
    add_definitions(
            -DWIN32=1
            -D_WIN32=1
    )
endif ()

#-----------------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------------

add_subdirectory(libs)

set(INCLUDELIST
        include/aboutmsg.h
        include/cullable.h
        include/editable.h
        include/iarchive.h
        include/ibrush.h
        include/icamera.h
        include/idatastream.h
        include/ieclass.h
        include/ientity.h
        include/ifilesystem.h
        include/ifiletypes.h
        include/ifilter.h
        include/igl.h
        include/iglrender.h
        include/igtkgl.h
        include/iimage.h
        include/imap.h
        include/imodel.h
        include/ipatch.h
        include/iplugin.h
        include/ireference.h
        include/irender.h
        include/iscenegraph.h
        include/iscriplib.h
        include/iselection.h
        include/ishaders.h
        include/itexdef.h
        include/itextstream.h
        include/itextures.h
        include/itoolbar.h
        include/iundo.h
        include/mapfile.h
        include/modelskin.h
        include/moduleobserver.h
        include/modulesystem.h
        include/nameable.h
        include/namespace.h
        include/preferencesystem.cpp
        include/preferencesystem.h
        include/qerplugin.h
        include/renderable.h
        include/selectable.h
        include/stream_version.h
        include/version.h
        include/warnings.h
        include/windowobserver.h
        )
add_library(includes ${INCLUDELIST})

#-----------------------------------------------------------------------
# Plugins
#-----------------------------------------------------------------------

if (NOT SKIP_RADIANT)
    add_subdirectory(contrib)
endif ()

#-----------------------------------------------------------------------
# Modules
#-----------------------------------------------------------------------

if (NOT SKIP_RADIANT)
    add_subdirectory(plugins)
endif ()

#-----------------------------------------------------------------------
# Radiant
#-----------------------------------------------------------------------

if (NOT SKIP_RADIANT)
    add_subdirectory(radiant build)
endif ()

if (CMAKE_EXECUTABLE_SUFFIX)
    set(RADIANT_EXECUTABLE ${CMAKE_EXECUTABLE_SUFFIX})
else ()
    execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE RADIANT_EXECUTABLE
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

if (NOT SKIP_RADIANT)
    set_target_properties(radiant PROPERTIES
            COMPILE_DEFINITIONS RADIANT_EXECUTABLE="${RADIANT_EXECUTABLE}"
            )
endif ()

#-----------------------------------------------------------------------
# Tools
#-----------------------------------------------------------------------

add_subdirectory(tools)

if (NOT (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR))
    # Copy data files from sources to the build directory
    message(STATUS "Copying data files")
    file(GLOB DATA_FILES "${PROJECT_SOURCE_DIR}/setup/data/tools/*")
    file(COPY ${DATA_FILES} DESTINATION "${PROJECT_BINARY_DIR}")
    file(GLOB DATA_FILES "${PROJECT_SOURCE_DIR}/docs/*")
    file(COPY ${DATA_FILES} DESTINATION "${PROJECT_BINARY_DIR}/docs")
endif ()

#-----------------------------------------------------------------------
# Game packs
#-----------------------------------------------------------------------

option(DOWNLOAD_GAMEPACKS "Download game packs" ON)
add_custom_target(game_packs
        COMMAND ${CMAKE_COMMAND} -E make_directory games
        COMMAND DOWNLOAD_GAMEPACKS=yes SOURCE_DIR="${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/install-gamepacks.sh" "${PROJECT_BINARY_DIR}"
        COMMENT "Downloading game packs"
        )
add_custom_command(OUTPUT games
        COMMAND make game_packs
        DEPENDS download-gamepacks.sh install-gamepacks.sh install-gamepack.sh
        )
if (DOWNLOAD_GAMEPACKS)
    add_custom_target(game_packs_all ALL DEPENDS games)
endif ()
