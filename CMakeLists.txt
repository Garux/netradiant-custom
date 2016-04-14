cmake_minimum_required(VERSION 2.8.11 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
# Enable Debug by default, can be changed with -DCMAKE_BUILD_TYPE=Release
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

project(NetRadiant C CXX)
option(BUILD_RADIANT "Build the gui" ON)

#-----------------------------------------------------------------------
# Version
#-----------------------------------------------------------------------

# CMake 3.0+ would allow this in project()
set(NetRadiant_VERSION_MAJOR 1)
set(NetRadiant_VERSION_MINOR 5)
set(NetRadiant_VERSION_PATCH 0)
set(NetRadiant_VERSION "${NetRadiant_VERSION_MAJOR}.${NetRadiant_VERSION_MINOR}.${NetRadiant_VERSION_PATCH}")

file(WRITE "${PROJECT_BINARY_DIR}/RADIANT_MAJOR" ${NetRadiant_VERSION_MAJOR})
file(WRITE "${PROJECT_BINARY_DIR}/RADIANT_MINOR" ${NetRadiant_VERSION_MINOR})

set(RADIANT_ABOUTMSG "Custom build" CACHE STRING "About message")

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
endif ()

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
        list(APPEND CMAKE_CXX_FLAGS --std=c++${CMAKE_CXX_STANDARD})
    else ()
        message(SEND_ERROR "Requires C++${CMAKE_CXX_STANDARD} or better")
    endif ()
else ()
    message(WARNING "Unrecognized compiler: ${CMAKE_CXX_COMPILER_ID}, make sure it supports C++${CMAKE_CXX_STANDARD}")
endif ()

#-----------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -fno-exceptions -fno-rtti")
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

#-----------------------------------------------------------------------
# Defs
#-----------------------------------------------------------------------

add_definitions(-DRADIANT_VERSION="${NetRadiant_VERSION}")
add_definitions(-DRADIANT_MAJOR_VERSION="${NetRadiant_VERSION_MAJOR}")
add_definitions(-DRADIANT_MINOR_VERSION="${NetRadiant_VERSION_MINOR}")

add_definitions(-DRADIANT_ABOUTMSG="${NetRadiant_ABOUT}")

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-D_DEBUG=1)
endif ()

if (APPLE)
    option(XWINDOWS "Build against X11" ON)
    add_definitions(
            -DPOSIX=1
    )
elseif (WIN32)
    add_definitions(
            -DWIN32=1
            -D_WIN32=1
    )
else ()
    set(XWINDOWS ON)
    add_definitions(
            -DPOSIX=1
    )
endif ()

if (XWINDOWS)
    find_package(X11 REQUIRED)
    include_directories(${X11_INCLUDE_DIR})
    add_definitions(-DXWINDOWS=1)
endif ()

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/libs")

#-----------------------------------------------------------------------
# Libraries
#-----------------------------------------------------------------------

add_subdirectory(libs)
add_subdirectory(include)

#-----------------------------------------------------------------------
# Plugins
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(contrib)
endif ()

#-----------------------------------------------------------------------
# Modules
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
    add_subdirectory(plugins)
endif ()

#-----------------------------------------------------------------------
# Radiant
#-----------------------------------------------------------------------

if (BUILD_RADIANT)
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

if (BUILD_RADIANT)
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
if (DOWNLOAD_GAMEPACKS)
    add_custom_target(game_packs_all ALL DEPENDS game_packs)
endif ()
