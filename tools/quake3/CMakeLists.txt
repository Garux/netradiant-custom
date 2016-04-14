include_directories(BEFORE common)

set(Q3MAP_VERSION 2.5.17n)
find_package(Git REQUIRED)
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (GIT_VERSION)
    set(Q3MAP_VERSION "${Q3MAP_VERSION}-git-${GIT_VERSION}")
endif ()
add_definitions(-DQ3MAP_VERSION="${Q3MAP_VERSION}")

find_package(GLIB REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIR})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

set(q3map2_games
        q3map2/game_darkplaces.h
        q3map2/game_dq.h
        q3map2/game_ef.h
        q3map2/game_etut.h
        q3map2/game_ja.h
        q3map2/game_jk2.h
        q3map2/game_nexuiz.h
        q3map2/game_prophecy.h
        q3map2/game_qfusion.h
        q3map2/game_quake3.h
        q3map2/game_quakelive.h
        q3map2/game_reaction.h
        q3map2/game_sof2.h
        q3map2/game_tenebrae.h
        q3map2/game_tremulous.h
        q3map2/game_wolf.h
        q3map2/game_wolfet.h
        q3map2/game_xonotic.h
        )

radiant_tool(q3map2
        common/cmdlib.c common/cmdlib.h
        common/imagelib.c common/imagelib.h
        common/inout.c common/inout.h
        common/jpeg.c
        common/md4.c common/md4.h
        common/mutex.c common/mutex.h
        common/polylib.c common/polylib.h
        common/polyset.h
        common/qfiles.h
        common/qthreads.h
        common/scriplib.c common/scriplib.h
        common/surfaceflags.h
        common/threads.c
        common/unzip.c common/unzip.h
        common/vfs.c common/vfs.h

        q3map2/brush.c
        q3map2/brush_primit.c
        q3map2/bsp.c
        q3map2/bsp_analyze.c
        q3map2/bsp_info.c
        q3map2/bsp_scale.c
        q3map2/bspfile_abstract.c
        q3map2/bspfile_ibsp.c
        q3map2/bspfile_rbsp.c
        q3map2/convert_ase.c
        q3map2/convert_bsp.c
        q3map2/convert_map.c
        q3map2/convert_obj.c
        q3map2/decals.c
        q3map2/exportents.c
        q3map2/facebsp.c
        q3map2/fixaas.c
        q3map2/fog.c
        ${q3map2_games} q3map2/game__null.h
        q3map2/help.c
        q3map2/image.c
        q3map2/leakfile.c
        q3map2/light.c
        q3map2/light_bounce.c
        q3map2/light_trace.c
        q3map2/light_ydnar.c
        q3map2/lightmaps_ydnar.c
        q3map2/main.c
        q3map2/map.c
        q3map2/mesh.c
        q3map2/minimap.c
        q3map2/model.c
        q3map2/patch.c
        q3map2/path_init.c
        q3map2/portals.c
        q3map2/prtfile.c
        q3map2/q3map2.h
        q3map2/shaders.c
        q3map2/surface.c
        q3map2/surface_extra.c
        q3map2/surface_foliage.c
        q3map2/surface_fur.c
        q3map2/surface_meta.c
        q3map2/tjunction.c
        q3map2/tree.c
        q3map2/vis.c
        q3map2/visflow.c
        q3map2/writebsp.c
        )

target_link_libraries(q3map2
        ${GLIB_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
        ${LIBXML2_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ddslib
        etclib
        filematch
        l_net
        mathlib
        picomodel
        )

radiant_tool(q3data
        common/aselib.c common/aselib.h
        common/bspfile.c common/bspfile.h
        common/cmdlib.c common/cmdlib.h
        common/imagelib.c common/imagelib.h
        common/inout.c common/inout.h
        common/md4.c common/md4.h
        common/scriplib.c common/scriplib.h
        common/trilib.c common/trilib.h
        common/unzip.c common/unzip.h
        common/vfs.c common/vfs.h

        q3data/3dslib.c q3data/3dslib.h
        q3data/compress.c
        q3data/images.c
        q3data/md3lib.c q3data/md3lib.h
        q3data/models.c
        q3data/p3dlib.c q3data/p3dlib.h
        q3data/polyset.c
        q3data/q3data.c q3data/q3data.h
        q3data/stripper.c
        q3data/video.c
        )

target_link_libraries(q3data
        ${GLIB_LIBRARIES}
        ${LIBXML2_LIBRARIES}
        ${ZLIB_LIBRARIES}
        filematch
        etclib
        l_net
        mathlib
        )

add_custom_target(quake3)
add_dependencies(quake3 q3map2 q3data)

if (UNIX)
    target_link_libraries(q3map2 pthread m)
    target_link_libraries(q3data m)
endif ()
