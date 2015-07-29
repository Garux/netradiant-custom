
include_directories(common)
include_directories("${CMAKE_SOURCE_DIR}/libs")
include_directories("${CMAKE_SOURCE_DIR}/include")

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(q2map EXCLUDE_FROM_ALL
   common/bspfile.c
   common/bspfile.h
   common/cmdlib.c
   common/cmdlib.h
   common/inout.c
   common/inout.h
   common/l3dslib.c
   common/l3dslib.h
   common/lbmlib.c
   common/lbmlib.h
   common/mathlib.c
   common/mathlib.h
   common/md4.c
   common/md4.h
   common/path_init.c
   common/polylib.c
   common/polylib.h
   common/scriplib.c
   common/scriplib.h
   common/threads.c
   common/trilib.c
   common/trilib.h
   q2map/brushbsp.c
   q2map/csg.c
   q2map/faces.c
   q2map/flow.c
#    q2map/gldraw.c
   q2map/glfile.c
   q2map/leakfile.c
   q2map/lightmap.c
   q2map/main.c
   q2map/map.c
   q2map/nodraw.c
   q2map/patches.c
   q2map/portals.c
   q2map/prtfile.c
   q2map/q2map.h
   q2map/qbsp.c
   q2map/qbsp.h
   q2map/qrad.c
   q2map/qrad.h
   q2map/qvis.c
   q2map/qvis.h
   q2map/textures.c
   q2map/trace.c
   q2map/tree.c
   q2map/writebsp.c
	"${CMAKE_BINARY_DIR}/version.c"
)
target_link_libraries(q2map
    ${LIBXML2_LIBRARIES}
    l_net
)

add_executable(qdata3 EXCLUDE_FROM_ALL
   common/bspfile.c
   common/bspfile.h
   common/cmdlib.c
   common/cmdlib.h
   common/inout.c
   common/inout.h
   common/l3dslib.c
   common/l3dslib.h
   common/lbmlib.c
   common/lbmlib.h
   common/mathlib.c
   common/mathlib.h
   common/md4.c
   common/md4.h
   common/path_init.c
   common/polylib.c
   common/polylib.h
   common/scriplib.c
   common/scriplib.h
   common/threads.c
   common/trilib.c
   common/trilib.h
   qdata/anorms.h
   qdata/images.c
   qdata/models.c
   qdata/qdata.c
   qdata/qdata.h
   qdata/sprites.c
   qdata/tables.c
   qdata/video.c
	"${CMAKE_BINARY_DIR}/version.c"
)

target_link_libraries(qdata3
    ${LIBXML2_LIBRARIES}
    l_net
)

# Why is the /common stuff duplicated for qdata_heretic2?
add_executable(h2data EXCLUDE_FROM_ALL
   qdata_heretic2/common/bspfile.c
   qdata_heretic2/common/bspfile.h
   qdata_heretic2/common/cmdlib.c
   qdata_heretic2/common/cmdlib.h
   qdata_heretic2/common/her2_threads.h
   qdata_heretic2/common/inout.c
   qdata_heretic2/common/inout.h
   qdata_heretic2/common/l3dslib.c
   qdata_heretic2/common/l3dslib.h
   qdata_heretic2/common/lbmlib.c
   qdata_heretic2/common/lbmlib.h
   qdata_heretic2/common/mathlib.c
   qdata_heretic2/common/mathlib.h
   qdata_heretic2/common/md4.c
   qdata_heretic2/common/md4.h
   qdata_heretic2/common/path_init.c
   qdata_heretic2/common/polylib.c
   qdata_heretic2/common/polylib.h
   qdata_heretic2/common/qfiles.c
   qdata_heretic2/common/qfiles.h
   qdata_heretic2/common/scriplib.c
   qdata_heretic2/common/scriplib.h
   qdata_heretic2/common/threads.c
   qdata_heretic2/common/token.c
   qdata_heretic2/common/token.h
   qdata_heretic2/common/trilib.c
   qdata_heretic2/common/trilib.h
   qdata_heretic2/qcommon/angles.h
   qdata_heretic2/qcommon/arrayedlist.h
   qdata_heretic2/qcommon/flex.h
   qdata_heretic2/qcommon/fmodel.h
   qdata_heretic2/qcommon/h2common.h
   qdata_heretic2/qcommon/placement.h
   qdata_heretic2/qcommon/q_typedef.h
   qdata_heretic2/qcommon/qfiles.h
   qdata_heretic2/qcommon/reference.c
   qdata_heretic2/qcommon/reference.h
   qdata_heretic2/qcommon/resourcemanager.c
   qdata_heretic2/qcommon/resourcemanager.h
   qdata_heretic2/qcommon/skeletons.c
   qdata_heretic2/qcommon/skeletons.h
   qdata_heretic2/adpcm.h
   qdata_heretic2/animcomp.c
   qdata_heretic2/animcomp.h
   qdata_heretic2/anorms.h
   qdata_heretic2/book.c
   qdata_heretic2/fmodels.c
   qdata_heretic2/images.c
   qdata_heretic2/jointed.c
   qdata_heretic2/jointed.h
   qdata_heretic2/joints.h
   qdata_heretic2/models.c
   qdata_heretic2/pics.c
   qdata_heretic2/qd_fmodel.h
   qdata_heretic2/qd_skeletons.c
   qdata_heretic2/qd_skeletons.h
   qdata_heretic2/qdata.c
   qdata_heretic2/qdata.h
   qdata_heretic2/resource.h
   qdata_heretic2/sprites.c
   qdata_heretic2/svdcmp.c
   qdata_heretic2/tables.c
   qdata_heretic2/tmix.c
   qdata_heretic2/video.c
	"${CMAKE_BINARY_DIR}/version.c"
)
target_include_directories(h2data BEFORE
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/qdata_heretic2/common"
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/qdata_heretic2/qcommon"
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/qdata_heretic2"
)
set_target_properties(h2data PROPERTIES RUNTIME_OUTPUT_DIRECTORY heretic2)

target_link_libraries(h2data
    ${LIBXML2_LIBRARIES}
    l_net
)
add_custom_target(quake2)
add_dependencies(quake2 q2map qdata3 h2data)

if (UNIX)
    target_link_libraries(q2map pthread m)
    target_link_libraries(qdata3 pthread m)
    target_link_libraries(h2data pthread m)
endif()


# A bunch of unused files:
#    common/q2_threads.h
#    common/qfiles.h
#    extra/bsp/bspinfo3/bspinfo3.c
#    extra/bsp/qbsp3/brushbsp.c
#    extra/bsp/qbsp3/csg.c
#    extra/bsp/qbsp3/faces.c
#    extra/bsp/qbsp3/gldraw.c
#    extra/bsp/qbsp3/glfile.c
#    extra/bsp/qbsp3/leakfile.c
#    extra/bsp/qbsp3/map.c
#    extra/bsp/qbsp3/nodraw.c
#    extra/bsp/qbsp3/portals.c
#    extra/bsp/qbsp3/prtfile.c
#    extra/bsp/qbsp3/qbsp.h
#    extra/bsp/qbsp3/qbsp3.c
#    extra/bsp/qbsp3/textures.c
#    extra/bsp/qbsp3/tree.c
#    extra/bsp/qbsp3/writebsp.c
#    extra/bsp/qrad3/lightmap.c
#    extra/bsp/qrad3/patches.c
#    extra/bsp/qrad3/qrad.h
#    extra/bsp/qrad3/qrad3.c
#    extra/bsp/qrad3/trace.c
#    extra/bsp/qvis3/flow.c
#    extra/bsp/qvis3/qvis3.c
#    extra/bsp/qvis3/vis.h
#    extra/common/bspfile.c
#    extra/common/bspfile.h
#    extra/common/cmdlib.c
#    extra/common/cmdlib.h
#    extra/common/l3dslib.c
#    extra/common/l3dslib.h
#    extra/common/lbmlib.c
#    extra/common/lbmlib.h
#    extra/common/mathlib.c
#    extra/common/mathlib.h
#    extra/common/mdfour.c
#    extra/common/mdfour.h
#    extra/common/polylib.c
#    extra/common/polylib.h
#    extra/common/qfiles.h
#    extra/common/scriplib.c
#    extra/common/scriplib.h
#    extra/common/threads.c
#    extra/common/threads.h
#    extra/common/trilib.c
#    extra/common/trilib.h
#    extra/qe4/brush.c
#    extra/qe4/brush.h
#    extra/qe4/bspfile.h
#    extra/qe4/camera.c
#    extra/qe4/camera.h
#    extra/qe4/cmdlib.c
#    extra/qe4/cmdlib.h
#    extra/qe4/csg.c
#    extra/qe4/drag.c
#    extra/qe4/eclass.c
#    extra/qe4/entity.c
#    extra/qe4/entity.h
#    extra/qe4/entityw.h
#    extra/qe4/glingr.h
#    extra/qe4/lbmlib.c
#    extra/qe4/lbmlib.h
#    extra/qe4/map.c
#    extra/qe4/map.h
#    extra/qe4/mathlib.c
#    extra/qe4/mathlib.h
#    extra/qe4/mru.c
#    extra/qe4/mru.h
#    extra/qe4/parse.c
#    extra/qe4/parse.h
#    extra/qe4/points.c
#    extra/qe4/qe3.c
#    extra/qe4/qe3.h
#    extra/qe4/qedefs.h
#    extra/qe4/qfiles.h
#    extra/qe4/resource.h
#    extra/qe4/select.c
#    extra/qe4/select.h
#    extra/qe4/textures.c
#    extra/qe4/textures.h
#    extra/qe4/vertsel.c
#    extra/qe4/view.h
#    extra/qe4/win_cam.c
#    extra/qe4/win_dlg.c
#    extra/qe4/win_ent.c
#    extra/qe4/win_main.c
#    extra/qe4/win_qe3.c
#    extra/qe4/win_xy.c
#    extra/qe4/win_z.c
#    extra/qe4/xy.c
#    extra/qe4/xy.h
#    extra/qe4/z.c
#    extra/qe4/z.h
#    extra/texpaint/resource.h
#    extra/texpaint/texpaint.c
#    extra/texpaint/texpaint.h
#    extra/texpaint/win_cam.c
#    extra/texpaint/win_main.c
#    extra/texpaint/win_pal.c
#    extra/texpaint/win_skin.c
