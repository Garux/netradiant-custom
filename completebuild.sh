# note: this is not a script yet
# will turn this into a working build script or makefile later
# maybe also replace install.sh

CFLAGS_COMMON="-DPOSIX -DXWINDOWS -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -g3 -D_DEBUG -fPIC -D_LINUX"
CXXFLAGS_COMMON="-Wno-non-virtual-dtor -Wreorder   -fno-exceptions -fno-rtti"

CFLAGS_QUAKE3_Q3MAP2="-c `xml2-config --cflags` `pkg-config glib-2.0 --cflags` `libpng-config --cflags` -Ibuild/debug/tools/quake3/common -Itools/quake3/common -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_CONTRIB="-c `pkg-config glib-2.0 --cflags` `pkg-config gtk+-2.0 --cflags`  -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_LIBPROFILE="-c -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_RADIANT="-c `pkg-config glib-2.0 --cflags` `xml2-config --cflags` `pkg-config gtk+-2.0 --cflags` `pkg-config gtkglext-1.0 --cflags` -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_GTKUTIL="-c `pkg-config glib-2.0 --cflags` `pkg-config gtk+-2.0 --cflags` `pkg-config gtkglext-1.0 --cflags` -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_JPEG6="-c -Ibuild/debug/libs/jpeg6 -Ilibs/jpeg6 -Ibuild/debug/libs -Ilibs"
CFLAGS_QUAKE2_QDATA_HERETIC2="-c `xml2-config --cflags` -Ibuild/debug/tools/quake2/qdata_heretic2/common -Itools/quake2/qdata_heretic2/common -Ibuild/debug/tools/quake2/qdata_heretic2/qcommon -Itools/quake2/qdata_heretic2/qcommon -Ibuild/debug/tools/quake2/qdata_heretic2 -Itools/quake2/qdata_heretic2 -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_QUAKE2_QDATA="-c `xml2-config --cflags` -Ibuild/debug/tools/quake2/common -Itools/quake2/common -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_PICOMODEL="-c -Ibuild/debug/libs -Ilibs"
CFLAGS_MODELPLUGIN="-c -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_MAPXML="-c `xml2-config --cflags` `pkg-config glib-2.0 --cflags`  -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"
CFLAGS_XML="-c `pkg-config glib-2.0 --cflags` `xml2-config --cflags` -Ibuild/debug/include -Iinclude -Ibuild/debug/libs -Ilibs"
CFLAGS_VFSPK3="-c `pkg-config glib-2.0 --cflags`  -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude"

LDFLAG_DYNAMICLIB="-fPIC -Wl,-fini,fini_stub -static-libgcc -ldl -shared"

gcc()
{
	/usr/bin/gcc $CFLAGS_COMMON "$@"
}

g++()
{
	/usr/bin/g++ $CFLAGS_COMMON $CXXFLAGS_COMMON "$@"
}

g++ -o build/debug/plugins/archivepak/plugin.os $CFLAGS_MODELPLUGIN plugins/archivepak/plugin.cpp
g++ -o build/debug/plugins/archivepak/archive.os $CFLAGS_MODELPLUGIN plugins/archivepak/archive.cpp
g++ -o build/debug/plugins/archivepak/pak.os $CFLAGS_MODELPLUGIN plugins/archivepak/pak.cpp
g++ -o build/debug/libs/cmdlib/cmdlib.o -c -pipe -DPOSIX -DXWINDOWS -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -Wno-non-virtual-dtor -Wreorder -g3 -D_DEBUG -fPIC -fno-exceptions -fno-rtti -Ibuild/debug/libs -Ilibs libs/cmdlib/cmdlib.cpp
ar rc build/debug/libs/libcmdlib.a build/debug/libs/cmdlib/cmdlib.o
ranlib build/debug/libs/libcmdlib.a
g++ -o build/debug/archivepak.so $LDFLAGS_DYNAMICLIB build/debug/plugins/archivepak/plugin.os build/debug/plugins/archivepak/archive.os build/debug/plugins/archivepak/pak.os -Lbuild/debug/libs -Llibs -lcmdlib
g++ -o build/debug/plugins/archivewad/plugin.os $CFLAGS_MODELPLUGIN plugins/archivewad/plugin.cpp
g++ -o build/debug/plugins/archivewad/archive.os $CFLAGS_MODELPLUGIN plugins/archivewad/archive.cpp
g++ -o build/debug/plugins/archivewad/wad.os $CFLAGS_MODELPLUGIN plugins/archivewad/wad.cpp
g++ -o build/debug/archivewad.so $LDFLAGS_DYNAMICLIB build/debug/plugins/archivewad/plugin.os build/debug/plugins/archivewad/archive.os build/debug/plugins/archivewad/wad.os -Lbuild/debug/libs -Llibs -lcmdlib
g++ -o build/debug/plugins/archivezip/plugin.os $CFLAGS_MODELPLUGIN plugins/archivezip/plugin.cpp
g++ -o build/debug/plugins/archivezip/archive.os $CFLAGS_MODELPLUGIN plugins/archivezip/archive.cpp
g++ -o build/debug/plugins/archivezip/pkzip.os $CFLAGS_MODELPLUGIN plugins/archivezip/pkzip.cpp
g++ -o build/debug/plugins/archivezip/zlibstream.os $CFLAGS_MODELPLUGIN plugins/archivezip/zlibstream.cpp
g++ -o build/debug/archivezip.so $LDFLAGS_DYNAMICLIB -lz build/debug/plugins/archivezip/plugin.os build/debug/plugins/archivezip/archive.os build/debug/plugins/archivezip/pkzip.os build/debug/plugins/archivezip/zlibstream.os -Lbuild/debug/libs -Llibs -lcmdlib
g++ -o build/debug/contrib/bobtoolz/dialogs/dialogs-gtk.os $CFLAGS_CONTRIB contrib/bobtoolz/dialogs/dialogs-gtk.cpp
g++ -o build/debug/contrib/bobtoolz/bobToolz-GTK.os $CFLAGS_CONTRIB contrib/bobtoolz/bobToolz-GTK.cpp
g++ -o build/debug/contrib/bobtoolz/bsploader.os $CFLAGS_CONTRIB contrib/bobtoolz/bsploader.cpp
g++ -o build/debug/contrib/bobtoolz/cportals.os $CFLAGS_CONTRIB contrib/bobtoolz/cportals.cpp
g++ -o build/debug/contrib/bobtoolz/DBobView.os $CFLAGS_CONTRIB contrib/bobtoolz/DBobView.cpp
g++ -o build/debug/contrib/bobtoolz/DBrush.os $CFLAGS_CONTRIB contrib/bobtoolz/DBrush.cpp
g++ -o build/debug/contrib/bobtoolz/DEntity.os $CFLAGS_CONTRIB contrib/bobtoolz/DEntity.cpp
g++ -o build/debug/contrib/bobtoolz/DEPair.os $CFLAGS_CONTRIB contrib/bobtoolz/DEPair.cpp
g++ -o build/debug/contrib/bobtoolz/DMap.os $CFLAGS_CONTRIB contrib/bobtoolz/DMap.cpp
g++ -o build/debug/contrib/bobtoolz/DPatch.os $CFLAGS_CONTRIB contrib/bobtoolz/DPatch.cpp
g++ -o build/debug/contrib/bobtoolz/DPlane.os $CFLAGS_CONTRIB contrib/bobtoolz/DPlane.cpp
g++ -o build/debug/contrib/bobtoolz/DPoint.os $CFLAGS_CONTRIB contrib/bobtoolz/DPoint.cpp
g++ -o build/debug/contrib/bobtoolz/DShape.os $CFLAGS_CONTRIB contrib/bobtoolz/DShape.cpp
g++ -o build/debug/contrib/bobtoolz/DTrainDrawer.os $CFLAGS_CONTRIB contrib/bobtoolz/DTrainDrawer.cpp
g++ -o build/debug/contrib/bobtoolz/DTreePlanter.os $CFLAGS_CONTRIB contrib/bobtoolz/DTreePlanter.cpp
g++ -o build/debug/contrib/bobtoolz/DVisDrawer.os $CFLAGS_CONTRIB contrib/bobtoolz/DVisDrawer.cpp
g++ -o build/debug/contrib/bobtoolz/DWinding.os $CFLAGS_CONTRIB contrib/bobtoolz/DWinding.cpp
g++ -o build/debug/contrib/bobtoolz/funchandlers-GTK.os $CFLAGS_CONTRIB contrib/bobtoolz/funchandlers-GTK.cpp
g++ -o build/debug/contrib/bobtoolz/lists.os $CFLAGS_CONTRIB contrib/bobtoolz/lists.cpp
g++ -o build/debug/contrib/bobtoolz/misc.os $CFLAGS_CONTRIB contrib/bobtoolz/misc.cpp
g++ -o build/debug/contrib/bobtoolz/ScriptParser.os $CFLAGS_CONTRIB contrib/bobtoolz/ScriptParser.cpp
g++ -o build/debug/contrib/bobtoolz/shapes.os $CFLAGS_CONTRIB contrib/bobtoolz/shapes.cpp
g++ -o build/debug/contrib/bobtoolz/visfind.os $CFLAGS_CONTRIB contrib/bobtoolz/visfind.cpp
gcc -o build/debug/libs/mathlib/mathlib.o $CFLAGS_PICOMODEL libs/mathlib/mathlib.c
gcc -o build/debug/libs/mathlib/bbox.o $CFLAGS_PICOMODEL libs/mathlib/bbox.c
gcc -o build/debug/libs/mathlib/line.o $CFLAGS_PICOMODEL libs/mathlib/line.c
gcc -o build/debug/libs/mathlib/m4x4.o $CFLAGS_PICOMODEL libs/mathlib/m4x4.c
gcc -o build/debug/libs/mathlib/ray.o $CFLAGS_PICOMODEL libs/mathlib/ray.c
ar rc build/debug/libs/libmathlib.a build/debug/libs/mathlib/mathlib.o build/debug/libs/mathlib/bbox.o build/debug/libs/mathlib/line.o build/debug/libs/mathlib/m4x4.o build/debug/libs/mathlib/ray.o
ranlib build/debug/libs/libmathlib.a
g++ -o build/debug/libs/profile/profile.o $CFLAGS_LIBPROFILE libs/profile/profile.cpp
g++ -o build/debug/libs/profile/file.o $CFLAGS_LIBPROFILE libs/profile/file.cpp
ar rc build/debug/libs/libprofile.a build/debug/libs/profile/profile.o build/debug/libs/profile/file.o
ranlib build/debug/libs/libprofile.a
g++ -o build/debug/bobtoolz.so $LDFLAG_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` build/debug/contrib/bobtoolz/dialogs/dialogs-gtk.os build/debug/contrib/bobtoolz/bobToolz-GTK.os build/debug/contrib/bobtoolz/bsploader.os build/debug/contrib/bobtoolz/cportals.os build/debug/contrib/bobtoolz/DBobView.os build/debug/contrib/bobtoolz/DBrush.os build/debug/contrib/bobtoolz/DEntity.os build/debug/contrib/bobtoolz/DEPair.os build/debug/contrib/bobtoolz/DMap.os build/debug/contrib/bobtoolz/DPatch.os build/debug/contrib/bobtoolz/DPlane.os build/debug/contrib/bobtoolz/DPoint.os build/debug/contrib/bobtoolz/DShape.os build/debug/contrib/bobtoolz/DTrainDrawer.os build/debug/contrib/bobtoolz/DTreePlanter.os build/debug/contrib/bobtoolz/DVisDrawer.os build/debug/contrib/bobtoolz/DWinding.os build/debug/contrib/bobtoolz/funchandlers-GTK.os build/debug/contrib/bobtoolz/lists.os build/debug/contrib/bobtoolz/misc.os build/debug/contrib/bobtoolz/ScriptParser.os build/debug/contrib/bobtoolz/shapes.os build/debug/contrib/bobtoolz/visfind.os -Lbuild/debug/libs -Llibs -lmathlib -lcmdlib -lprofile
g++ -o build/debug/contrib/brushexport/plugin.os $CFLAGS_CONTRIB contrib/brushexport/plugin.cpp
g++ -o build/debug/contrib/brushexport/interface.os $CFLAGS_CONTRIB contrib/brushexport/interface.cpp
g++ -o build/debug/contrib/brushexport/callbacks.os $CFLAGS_CONTRIB contrib/brushexport/callbacks.cpp
g++ -o build/debug/contrib/brushexport/support.os $CFLAGS_CONTRIB contrib/brushexport/support.cpp
g++ -o build/debug/contrib/brushexport/export.os $CFLAGS_CONTRIB contrib/brushexport/export.cpp
g++ -o build/debug/brushexport.so $LDFLAG_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` build/debug/contrib/brushexport/plugin.os build/debug/contrib/brushexport/interface.os build/debug/contrib/brushexport/callbacks.os build/debug/contrib/brushexport/support.os build/debug/contrib/brushexport/export.os -Lbuild/debug/libs -Llibs
g++ -o build/debug/contrib/prtview/AboutDialog.os $CFLAGS_CONTRIB contrib/prtview/AboutDialog.cpp
g++ -o build/debug/contrib/prtview/ConfigDialog.os $CFLAGS_CONTRIB contrib/prtview/ConfigDialog.cpp
g++ -o build/debug/contrib/prtview/LoadPortalFileDialog.os $CFLAGS_CONTRIB contrib/prtview/LoadPortalFileDialog.cpp
g++ -o build/debug/contrib/prtview/portals.os $CFLAGS_CONTRIB contrib/prtview/portals.cpp
g++ -o build/debug/contrib/prtview/prtview.os $CFLAGS_CONTRIB contrib/prtview/prtview.cpp
g++ -o build/debug/contrib/shaderplug/shaderplug.os -c -pipe -DPOSIX -DXWINDOWS -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -Wno-non-virtual-dtor -Wreorder -g3 -D_DEBUG -fPIC -fno-exceptions -fno-rtti `pkg-config glib-2.0 --cflags` `pkg-config gtk+-2.0 --cflags` `xml2-config --cflags` -fPIC -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude contrib/shaderplug/shaderplug.cpp
g++ -o build/debug/contrib/sunplug/sunplug.os $CFLAGS_CONTRIB contrib/sunplug/sunplug.cpp
g++ -o build/debug/contrib/ufoaiplug/ufoai.os $CFLAGS_CONTRIB contrib/ufoaiplug/ufoai.cpp
g++ -o build/debug/contrib/ufoaiplug/ufoai_filters.os $CFLAGS_CONTRIB contrib/ufoaiplug/ufoai_filters.cpp
g++ -o build/debug/contrib/ufoaiplug/ufoai_gtk.os $CFLAGS_CONTRIB contrib/ufoaiplug/ufoai_gtk.cpp
g++ -o build/debug/contrib/ufoaiplug/ufoai_level.os $CFLAGS_CONTRIB contrib/ufoaiplug/ufoai_level.cpp
g++ -o build/debug/plugins/entity/plugin.os $CFLAGS_MODELPLUGIN plugins/entity/plugin.cpp
g++ -o build/debug/plugins/entity/entity.os $CFLAGS_MODELPLUGIN plugins/entity/entity.cpp
g++ -o build/debug/plugins/entity/eclassmodel.os $CFLAGS_MODELPLUGIN plugins/entity/eclassmodel.cpp
g++ -o build/debug/plugins/entity/generic.os $CFLAGS_MODELPLUGIN plugins/entity/generic.cpp
g++ -o build/debug/plugins/entity/group.os $CFLAGS_MODELPLUGIN plugins/entity/group.cpp
g++ -o build/debug/plugins/entity/light.os $CFLAGS_MODELPLUGIN plugins/entity/light.cpp
g++ -o build/debug/plugins/entity/miscmodel.os $CFLAGS_MODELPLUGIN plugins/entity/miscmodel.cpp
g++ -o build/debug/plugins/entity/doom3group.os $CFLAGS_MODELPLUGIN plugins/entity/doom3group.cpp
g++ -o build/debug/plugins/entity/skincache.os $CFLAGS_MODELPLUGIN plugins/entity/skincache.cpp
g++ -o build/debug/plugins/entity/angle.os $CFLAGS_MODELPLUGIN plugins/entity/angle.cpp
g++ -o build/debug/plugins/entity/angles.os $CFLAGS_MODELPLUGIN plugins/entity/angles.cpp
g++ -o build/debug/plugins/entity/colour.os $CFLAGS_MODELPLUGIN plugins/entity/colour.cpp
g++ -o build/debug/plugins/entity/filters.os $CFLAGS_MODELPLUGIN plugins/entity/filters.cpp
g++ -o build/debug/plugins/entity/model.os $CFLAGS_MODELPLUGIN plugins/entity/model.cpp
g++ -o build/debug/plugins/entity/namedentity.os $CFLAGS_MODELPLUGIN plugins/entity/namedentity.cpp
g++ -o build/debug/plugins/entity/origin.os $CFLAGS_MODELPLUGIN plugins/entity/origin.cpp
g++ -o build/debug/plugins/entity/scale.os $CFLAGS_MODELPLUGIN plugins/entity/scale.cpp
g++ -o build/debug/plugins/entity/targetable.os $CFLAGS_MODELPLUGIN plugins/entity/targetable.cpp
g++ -o build/debug/plugins/entity/rotation.os $CFLAGS_MODELPLUGIN plugins/entity/rotation.cpp
g++ -o build/debug/plugins/entity/modelskinkey.os $CFLAGS_MODELPLUGIN plugins/entity/modelskinkey.cpp
g++ -o build/debug/entity.so $LDFLAGS_DYNAMICLIB build/debug/plugins/entity/plugin.os build/debug/plugins/entity/entity.os build/debug/plugins/entity/eclassmodel.os build/debug/plugins/entity/generic.os build/debug/plugins/entity/group.os build/debug/plugins/entity/light.os build/debug/plugins/entity/miscmodel.os build/debug/plugins/entity/doom3group.os build/debug/plugins/entity/skincache.os build/debug/plugins/entity/angle.os build/debug/plugins/entity/angles.os build/debug/plugins/entity/colour.os build/debug/plugins/entity/filters.os build/debug/plugins/entity/model.os build/debug/plugins/entity/namedentity.os build/debug/plugins/entity/origin.os build/debug/plugins/entity/scale.os build/debug/plugins/entity/targetable.os build/debug/plugins/entity/rotation.os build/debug/plugins/entity/modelskinkey.os -Lbuild/debug -L.
gcc -o build/debug/tools/quake2/qdata_heretic2/common/bspfile.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/bspfile.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/cmdlib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/cmdlib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/inout.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/inout.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/l3dslib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/l3dslib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/lbmlib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/lbmlib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/mathlib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/mathlib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/md4.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/md4.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/path_init.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/path_init.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/qfiles.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/qfiles.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/scriplib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/scriplib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/threads.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/threads.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/token.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/token.c
gcc -o build/debug/tools/quake2/qdata_heretic2/common/trilib.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/common/trilib.c
gcc -o build/debug/tools/quake2/qdata_heretic2/qcommon/reference.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/qcommon/reference.c
gcc -o build/debug/tools/quake2/qdata_heretic2/qcommon/resourcemanager.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/qcommon/resourcemanager.c
gcc -o build/debug/tools/quake2/qdata_heretic2/qcommon/skeletons.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/qcommon/skeletons.c
gcc -o build/debug/tools/quake2/qdata_heretic2/animcomp.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/animcomp.c
gcc -o build/debug/tools/quake2/qdata_heretic2/book.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/book.c
gcc -o build/debug/tools/quake2/qdata_heretic2/fmodels.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/fmodels.c
gcc -o build/debug/tools/quake2/qdata_heretic2/images.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/images.c
gcc -o build/debug/tools/quake2/qdata_heretic2/jointed.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/jointed.c
gcc -o build/debug/tools/quake2/qdata_heretic2/models.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/models.c
gcc -o build/debug/tools/quake2/qdata_heretic2/pics.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/pics.c
gcc -o build/debug/tools/quake2/qdata_heretic2/qdata.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/qdata.c
gcc -o build/debug/tools/quake2/qdata_heretic2/qd_skeletons.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/qd_skeletons.c
gcc -o build/debug/tools/quake2/qdata_heretic2/sprites.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/sprites.c
gcc -o build/debug/tools/quake2/qdata_heretic2/svdcmp.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/svdcmp.c
gcc -o build/debug/tools/quake2/qdata_heretic2/tables.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/tables.c
gcc -o build/debug/tools/quake2/qdata_heretic2/tmix.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/tmix.c
gcc -o build/debug/tools/quake2/qdata_heretic2/video.o $CFLAGS_QUAKE2_QDATA_HERETIC2 tools/quake2/qdata_heretic2/video.c
gcc -o build/debug/libs/l_net/l_net.o $CFLAGS_PICOMODEL libs/l_net/l_net.c
gcc -o build/debug/libs/l_net/l_net_berkley.o $CFLAGS_PICOMODEL libs/l_net/l_net_berkley.c
ar rc build/debug/libs/libl_net.a build/debug/libs/l_net/l_net.o build/debug/libs/l_net/l_net_berkley.o
ranlib build/debug/libs/libl_net.a
g++ -o build/debug/h2data -fPIC -Wl,-fini,fini_stub -L. -static-libgcc `xml2-config --libs` -lpthread build/debug/tools/quake2/qdata_heretic2/common/bspfile.o build/debug/tools/quake2/qdata_heretic2/common/cmdlib.o build/debug/tools/quake2/qdata_heretic2/common/inout.o build/debug/tools/quake2/qdata_heretic2/common/l3dslib.o build/debug/tools/quake2/qdata_heretic2/common/lbmlib.o build/debug/tools/quake2/qdata_heretic2/common/mathlib.o build/debug/tools/quake2/qdata_heretic2/common/md4.o build/debug/tools/quake2/qdata_heretic2/common/path_init.o build/debug/tools/quake2/qdata_heretic2/common/qfiles.o build/debug/tools/quake2/qdata_heretic2/common/scriplib.o build/debug/tools/quake2/qdata_heretic2/common/threads.o build/debug/tools/quake2/qdata_heretic2/common/token.o build/debug/tools/quake2/qdata_heretic2/common/trilib.o build/debug/tools/quake2/qdata_heretic2/qcommon/reference.o build/debug/tools/quake2/qdata_heretic2/qcommon/resourcemanager.o build/debug/tools/quake2/qdata_heretic2/qcommon/skeletons.o build/debug/tools/quake2/qdata_heretic2/animcomp.o build/debug/tools/quake2/qdata_heretic2/book.o build/debug/tools/quake2/qdata_heretic2/fmodels.o build/debug/tools/quake2/qdata_heretic2/images.o build/debug/tools/quake2/qdata_heretic2/jointed.o build/debug/tools/quake2/qdata_heretic2/models.o build/debug/tools/quake2/qdata_heretic2/pics.o build/debug/tools/quake2/qdata_heretic2/qdata.o build/debug/tools/quake2/qdata_heretic2/qd_skeletons.o build/debug/tools/quake2/qdata_heretic2/sprites.o build/debug/tools/quake2/qdata_heretic2/svdcmp.o build/debug/tools/quake2/qdata_heretic2/tables.o build/debug/tools/quake2/qdata_heretic2/tmix.o build/debug/tools/quake2/qdata_heretic2/video.o -Lbuild/debug/libs -Llibs -ll_net
g++ -o build/debug/plugins/image/bmp.os $CFLAGS_MODELPLUGIN plugins/image/bmp.cpp
g++ -o build/debug/plugins/image/jpeg.os $CFLAGS_MODELPLUGIN plugins/image/jpeg.cpp
g++ -o build/debug/plugins/image/image.os $CFLAGS_MODELPLUGIN plugins/image/image.cpp
g++ -o build/debug/plugins/image/pcx.os $CFLAGS_MODELPLUGIN plugins/image/pcx.cpp
g++ -o build/debug/plugins/image/tga.os $CFLAGS_MODELPLUGIN plugins/image/tga.cpp
g++ -o build/debug/plugins/image/dds.os $CFLAGS_MODELPLUGIN plugins/image/dds.cpp
g++ -o build/debug/libs/jpeg6/jcomapi.o $CFLAGS_JPEG6 libs/jpeg6/jcomapi.cpp
g++ -o build/debug/libs/jpeg6/jdcoefct.o $CFLAGS_JPEG6 libs/jpeg6/jdcoefct.cpp
g++ -o build/debug/libs/jpeg6/jdinput.o $CFLAGS_JPEG6 libs/jpeg6/jdinput.cpp
g++ -o build/debug/libs/jpeg6/jdpostct.o $CFLAGS_JPEG6 libs/jpeg6/jdpostct.cpp
g++ -o build/debug/libs/jpeg6/jfdctflt.o $CFLAGS_JPEG6 libs/jpeg6/jfdctflt.cpp
g++ -o build/debug/libs/jpeg6/jpgload.o $CFLAGS_JPEG6 libs/jpeg6/jpgload.cpp
g++ -o build/debug/libs/jpeg6/jdapimin.o $CFLAGS_JPEG6 libs/jpeg6/jdapimin.cpp
g++ -o build/debug/libs/jpeg6/jdcolor.o $CFLAGS_JPEG6 libs/jpeg6/jdcolor.cpp
g++ -o build/debug/libs/jpeg6/jdmainct.o $CFLAGS_JPEG6 libs/jpeg6/jdmainct.cpp
g++ -o build/debug/libs/jpeg6/jdsample.o $CFLAGS_JPEG6 libs/jpeg6/jdsample.cpp
g++ -o build/debug/libs/jpeg6/jidctflt.o $CFLAGS_JPEG6 libs/jpeg6/jidctflt.cpp
g++ -o build/debug/libs/jpeg6/jutils.o $CFLAGS_JPEG6 libs/jpeg6/jutils.cpp
g++ -o build/debug/libs/jpeg6/jdapistd.o $CFLAGS_JPEG6 libs/jpeg6/jdapistd.cpp
g++ -o build/debug/libs/jpeg6/jddctmgr.o $CFLAGS_JPEG6 libs/jpeg6/jddctmgr.cpp
g++ -o build/debug/libs/jpeg6/jdmarker.o $CFLAGS_JPEG6 libs/jpeg6/jdmarker.cpp
g++ -o build/debug/libs/jpeg6/jdtrans.o $CFLAGS_JPEG6 libs/jpeg6/jdtrans.cpp
g++ -o build/debug/libs/jpeg6/jmemmgr.o $CFLAGS_JPEG6 libs/jpeg6/jmemmgr.cpp
g++ -o build/debug/libs/jpeg6/jdatasrc.o $CFLAGS_JPEG6 libs/jpeg6/jdatasrc.cpp
g++ -o build/debug/libs/jpeg6/jdhuff.o $CFLAGS_JPEG6 libs/jpeg6/jdhuff.cpp
g++ -o build/debug/libs/jpeg6/jdmaster.o $CFLAGS_JPEG6 libs/jpeg6/jdmaster.cpp
g++ -o build/debug/libs/jpeg6/jerror.o $CFLAGS_JPEG6 libs/jpeg6/jerror.cpp
g++ -o build/debug/libs/jpeg6/jmemnobs.o $CFLAGS_JPEG6 libs/jpeg6/jmemnobs.cpp
ar rc build/debug/libs/libjpeg6.a build/debug/libs/jpeg6/jcomapi.o build/debug/libs/jpeg6/jdcoefct.o build/debug/libs/jpeg6/jdinput.o build/debug/libs/jpeg6/jdpostct.o build/debug/libs/jpeg6/jfdctflt.o build/debug/libs/jpeg6/jpgload.o build/debug/libs/jpeg6/jdapimin.o build/debug/libs/jpeg6/jdcolor.o build/debug/libs/jpeg6/jdmainct.o build/debug/libs/jpeg6/jdsample.o build/debug/libs/jpeg6/jidctflt.o build/debug/libs/jpeg6/jutils.o build/debug/libs/jpeg6/jdapistd.o build/debug/libs/jpeg6/jddctmgr.o build/debug/libs/jpeg6/jdmarker.o build/debug/libs/jpeg6/jdtrans.o build/debug/libs/jpeg6/jmemmgr.o build/debug/libs/jpeg6/jdatasrc.o build/debug/libs/jpeg6/jdhuff.o build/debug/libs/jpeg6/jdmaster.o build/debug/libs/jpeg6/jerror.o build/debug/libs/jpeg6/jmemnobs.o
ranlib build/debug/libs/libjpeg6.a
gcc -o build/debug/libs/ddslib/ddslib.o $CFLAGS_PICOMODEL libs/ddslib/ddslib.c
ar rc build/debug/libs/libddslib.a build/debug/libs/ddslib/ddslib.o
ranlib build/debug/libs/libddslib.a
g++ -o build/debug/image.so $LDFLAGS_DYNAMICLIB build/debug/plugins/image/bmp.os build/debug/plugins/image/jpeg.os build/debug/plugins/image/image.os build/debug/plugins/image/pcx.os build/debug/plugins/image/tga.os build/debug/plugins/image/dds.os -Lbuild/debug/libs -Llibs -ljpeg6 -lddslib
g++ -o build/debug/plugins/imagehl/imagehl.os $CFLAGS_MODELPLUGIN plugins/imagehl/imagehl.cpp
g++ -o build/debug/plugins/imagehl/hlw.os $CFLAGS_MODELPLUGIN plugins/imagehl/hlw.cpp
g++ -o build/debug/plugins/imagehl/mip.os $CFLAGS_MODELPLUGIN plugins/imagehl/mip.cpp
g++ -o build/debug/plugins/imagehl/sprite.os $CFLAGS_MODELPLUGIN plugins/imagehl/sprite.cpp
g++ -o build/debug/imagehl.so $LDFLAGS_DYNAMICLIB build/debug/plugins/imagehl/imagehl.os build/debug/plugins/imagehl/hlw.os build/debug/plugins/imagehl/mip.os build/debug/plugins/imagehl/sprite.os -Lbuild/debug -L.
g++ -o build/debug/plugins/imagepng/plugin.os -c -pipe -DPOSIX -DXWINDOWS -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -Wno-non-virtual-dtor -Wreorder -g3 -D_DEBUG -fPIC -fno-exceptions -fno-rtti `libpng-config --cflags` -fPIC -Ibuild/debug/libs -Ilibs -Ibuild/debug/include -Iinclude plugins/imagepng/plugin.cpp
g++ -o build/debug/imagepng.so $LDFLAG_DYNAMICLIB `libpng-config --ldflags` build/debug/plugins/imagepng/plugin.os -Lbuild/debug -L.
g++ -o build/debug/plugins/imageq2/imageq2.os $CFLAGS_MODELPLUGIN plugins/imageq2/imageq2.cpp
g++ -o build/debug/plugins/imageq2/wal.os $CFLAGS_MODELPLUGIN plugins/imageq2/wal.cpp
g++ -o build/debug/plugins/imageq2/wal32.os $CFLAGS_MODELPLUGIN plugins/imageq2/wal32.cpp
g++ -o build/debug/imageq2.so $LDFLAGS_DYNAMICLIB build/debug/plugins/imageq2/imageq2.os build/debug/plugins/imageq2/wal.os build/debug/plugins/imageq2/wal32.os -Lbuild/debug -L.
g++ -o build/debug/libs/gtkutil/accelerator.o $CFLAGS_GTKUTIL libs/gtkutil/accelerator.cpp
g++ -o build/debug/libs/gtkutil/button.o $CFLAGS_GTKUTIL libs/gtkutil/button.cpp
g++ -o build/debug/libs/gtkutil/clipboard.o $CFLAGS_GTKUTIL libs/gtkutil/clipboard.cpp
g++ -o build/debug/libs/gtkutil/closure.o $CFLAGS_GTKUTIL libs/gtkutil/closure.cpp
g++ -o build/debug/libs/gtkutil/container.o $CFLAGS_GTKUTIL libs/gtkutil/container.cpp
g++ -o build/debug/libs/gtkutil/cursor.o $CFLAGS_GTKUTIL libs/gtkutil/cursor.cpp
g++ -o build/debug/libs/gtkutil/dialog.o $CFLAGS_GTKUTIL libs/gtkutil/dialog.cpp
g++ -o build/debug/libs/gtkutil/entry.o $CFLAGS_GTKUTIL libs/gtkutil/entry.cpp
g++ -o build/debug/libs/gtkutil/filechooser.o $CFLAGS_GTKUTIL libs/gtkutil/filechooser.cpp
g++ -o build/debug/libs/gtkutil/frame.o $CFLAGS_GTKUTIL libs/gtkutil/frame.cpp
g++ -o build/debug/libs/gtkutil/glfont.o $CFLAGS_GTKUTIL libs/gtkutil/glfont.cpp
g++ -o build/debug/libs/gtkutil/glwidget.o $CFLAGS_GTKUTIL libs/gtkutil/glwidget.cpp
g++ -o build/debug/libs/gtkutil/idledraw.o $CFLAGS_GTKUTIL libs/gtkutil/idledraw.cpp
g++ -o build/debug/libs/gtkutil/image.o $CFLAGS_GTKUTIL libs/gtkutil/image.cpp
g++ -o build/debug/libs/gtkutil/menu.o $CFLAGS_GTKUTIL libs/gtkutil/menu.cpp
g++ -o build/debug/libs/gtkutil/messagebox.o $CFLAGS_GTKUTIL libs/gtkutil/messagebox.cpp
g++ -o build/debug/libs/gtkutil/nonmodal.o $CFLAGS_GTKUTIL libs/gtkutil/nonmodal.cpp
g++ -o build/debug/libs/gtkutil/paned.o $CFLAGS_GTKUTIL libs/gtkutil/paned.cpp
g++ -o build/debug/libs/gtkutil/pointer.o $CFLAGS_GTKUTIL libs/gtkutil/pointer.cpp
g++ -o build/debug/libs/gtkutil/toolbar.o $CFLAGS_GTKUTIL libs/gtkutil/toolbar.cpp
g++ -o build/debug/libs/gtkutil/widget.o $CFLAGS_GTKUTIL libs/gtkutil/widget.cpp
g++ -o build/debug/libs/gtkutil/window.o $CFLAGS_GTKUTIL libs/gtkutil/window.cpp
g++ -o build/debug/libs/gtkutil/xorrectangle.o $CFLAGS_GTKUTIL libs/gtkutil/xorrectangle.cpp
ar rc build/debug/libs/libgtkutil.a build/debug/libs/gtkutil/accelerator.o build/debug/libs/gtkutil/button.o build/debug/libs/gtkutil/clipboard.o build/debug/libs/gtkutil/closure.o build/debug/libs/gtkutil/container.o build/debug/libs/gtkutil/cursor.o build/debug/libs/gtkutil/dialog.o build/debug/libs/gtkutil/entry.o build/debug/libs/gtkutil/frame.o build/debug/libs/gtkutil/filechooser.o build/debug/libs/gtkutil/glfont.o build/debug/libs/gtkutil/glwidget.o build/debug/libs/gtkutil/image.o build/debug/libs/gtkutil/idledraw.o build/debug/libs/gtkutil/menu.o build/debug/libs/gtkutil/messagebox.o build/debug/libs/gtkutil/nonmodal.o build/debug/libs/gtkutil/paned.o build/debug/libs/gtkutil/pointer.o build/debug/libs/gtkutil/toolbar.o build/debug/libs/gtkutil/widget.o build/debug/libs/gtkutil/window.o build/debug/libs/gtkutil/xorrectangle.o
ranlib build/debug/libs/libgtkutil.a
gcc -o build/debug/libs/md5lib/md5lib.o $CFLAGS_PICOMODEL libs/md5lib/md5lib.c
ar rc build/debug/libs/libmd5lib.a build/debug/libs/md5lib/md5lib.o
ranlib build/debug/libs/libmd5lib.a
gcc -o build/debug/libs/picomodel/picointernal.o $CFLAGS_PICOMODEL libs/picomodel/picointernal.c
gcc -o build/debug/libs/picomodel/picomodel.o $CFLAGS_PICOMODEL libs/picomodel/picomodel.c
gcc -o build/debug/libs/picomodel/picomodules.o $CFLAGS_PICOMODEL libs/picomodel/picomodules.c
gcc -o build/debug/libs/picomodel/pm_3ds.o $CFLAGS_PICOMODEL libs/picomodel/pm_3ds.c
gcc -o build/debug/libs/picomodel/pm_ase.o $CFLAGS_PICOMODEL libs/picomodel/pm_ase.c
gcc -o build/debug/libs/picomodel/pm_md3.o $CFLAGS_PICOMODEL libs/picomodel/pm_md3.c
gcc -o build/debug/libs/picomodel/pm_obj.o $CFLAGS_PICOMODEL libs/picomodel/pm_obj.c
gcc -o build/debug/libs/picomodel/pm_ms3d.o $CFLAGS_PICOMODEL libs/picomodel/pm_ms3d.c
gcc -o build/debug/libs/picomodel/pm_mdc.o $CFLAGS_PICOMODEL libs/picomodel/pm_mdc.c
gcc -o build/debug/libs/picomodel/pm_fm.o $CFLAGS_PICOMODEL libs/picomodel/pm_fm.c
gcc -o build/debug/libs/picomodel/pm_md2.o $CFLAGS_PICOMODEL libs/picomodel/pm_md2.c
gcc -o build/debug/libs/picomodel/pm_lwo.o $CFLAGS_PICOMODEL libs/picomodel/pm_lwo.c
gcc -o build/debug/libs/picomodel/pm_terrain.o $CFLAGS_PICOMODEL libs/picomodel/pm_terrain.c
gcc -o build/debug/libs/picomodel/lwo/clip.o $CFLAGS_PICOMODEL libs/picomodel/lwo/clip.c
gcc -o build/debug/libs/picomodel/lwo/envelope.o $CFLAGS_PICOMODEL libs/picomodel/lwo/envelope.c
gcc -o build/debug/libs/picomodel/lwo/list.o $CFLAGS_PICOMODEL libs/picomodel/lwo/list.c
gcc -o build/debug/libs/picomodel/lwo/lwio.o $CFLAGS_PICOMODEL libs/picomodel/lwo/lwio.c
gcc -o build/debug/libs/picomodel/lwo/lwo2.o $CFLAGS_PICOMODEL libs/picomodel/lwo/lwo2.c
gcc -o build/debug/libs/picomodel/lwo/lwob.o $CFLAGS_PICOMODEL libs/picomodel/lwo/lwob.c
gcc -o build/debug/libs/picomodel/lwo/pntspols.o $CFLAGS_PICOMODEL libs/picomodel/lwo/pntspols.c
gcc -o build/debug/libs/picomodel/lwo/surface.o $CFLAGS_PICOMODEL libs/picomodel/lwo/surface.c
gcc -o build/debug/libs/picomodel/lwo/vecmath.o $CFLAGS_PICOMODEL libs/picomodel/lwo/vecmath.c
gcc -o build/debug/libs/picomodel/lwo/vmap.o $CFLAGS_PICOMODEL libs/picomodel/lwo/vmap.c
ar rc build/debug/libs/libpicomodel.a build/debug/libs/picomodel/picointernal.o build/debug/libs/picomodel/picomodel.o build/debug/libs/picomodel/picomodules.o build/debug/libs/picomodel/pm_3ds.o build/debug/libs/picomodel/pm_ase.o build/debug/libs/picomodel/pm_md3.o build/debug/libs/picomodel/pm_obj.o build/debug/libs/picomodel/pm_ms3d.o build/debug/libs/picomodel/pm_mdc.o build/debug/libs/picomodel/pm_fm.o build/debug/libs/picomodel/pm_md2.o build/debug/libs/picomodel/pm_lwo.o build/debug/libs/picomodel/pm_terrain.o build/debug/libs/picomodel/lwo/clip.o build/debug/libs/picomodel/lwo/envelope.o build/debug/libs/picomodel/lwo/list.o build/debug/libs/picomodel/lwo/lwio.o build/debug/libs/picomodel/lwo/lwo2.o build/debug/libs/picomodel/lwo/lwob.o build/debug/libs/picomodel/lwo/pntspols.o build/debug/libs/picomodel/lwo/surface.o build/debug/libs/picomodel/lwo/vecmath.o build/debug/libs/picomodel/lwo/vmap.o
ranlib build/debug/libs/libpicomodel.a
g++ -o build/debug/libs/xml/ixml.o $CFLAGS_AA libs/xml/ixml.cpp
g++ -o build/debug/libs/xml/xmlparser.o $CFLAGS_AA libs/xml/xmlparser.cpp
g++ -o build/debug/libs/xml/xmlwriter.o $CFLAGS_AA libs/xml/xmlwriter.cpp
g++ -o build/debug/libs/xml/xmlelement.o $CFLAGS_AA libs/xml/xmlelement.cpp
g++ -o build/debug/libs/xml/xmltextags.o $CFLAGS_AA libs/xml/xmltextags.cpp
ar rc build/debug/libs/libxmllib.a build/debug/libs/xml/ixml.o build/debug/libs/xml/xmlparser.o build/debug/libs/xml/xmlwriter.o build/debug/libs/xml/xmlelement.o build/debug/libs/xml/xmltextags.o
ranlib build/debug/libs/libxmllib.a
g++ -o build/debug/plugins/mapq3/plugin.os $CFLAGS_MODELPLUGIN plugins/mapq3/plugin.cpp
g++ -o build/debug/plugins/mapq3/parse.os $CFLAGS_MODELPLUGIN plugins/mapq3/parse.cpp
g++ -o build/debug/plugins/mapq3/write.os $CFLAGS_MODELPLUGIN plugins/mapq3/write.cpp
g++ -o build/debug/mapq3.so $LDFLAGS_DYNAMICLIB build/debug/plugins/mapq3/plugin.os build/debug/plugins/mapq3/parse.os build/debug/plugins/mapq3/write.os -Lbuild/debug/libs -Llibs -lcmdlib
g++ -o build/debug/plugins/mapxml/plugin.os $CFLAGS_1 plugins/mapxml/plugin.cpp
g++ -o build/debug/plugins/mapxml/xmlparse.os $CFLAGS_1 plugins/mapxml/xmlparse.cpp
g++ -o build/debug/plugins/mapxml/xmlwrite.os $CFLAGS_1 plugins/mapxml/xmlwrite.cpp
g++ -o build/debug/mapxml.so $LDFLAGS_DYNAMICLIB `xml2-config --libs` `pkg-config glib-2.0 --libs` build/debug/plugins/mapxml/plugin.os build/debug/plugins/mapxml/xmlparse.os build/debug/plugins/mapxml/xmlwrite.os -Lbuild/debug -L.
g++ -o build/debug/plugins/md3model/plugin.os $CFLAGS_MODELPLUGIN plugins/md3model/plugin.cpp
g++ -o build/debug/plugins/md3model/mdl.os $CFLAGS_MODELPLUGIN plugins/md3model/mdl.cpp
g++ -o build/debug/plugins/md3model/md3.os $CFLAGS_MODELPLUGIN plugins/md3model/md3.cpp
g++ -o build/debug/plugins/md3model/md2.os $CFLAGS_MODELPLUGIN plugins/md3model/md2.cpp
g++ -o build/debug/plugins/md3model/mdc.os $CFLAGS_MODELPLUGIN plugins/md3model/mdc.cpp
g++ -o build/debug/plugins/md3model/mdlimage.os $CFLAGS_MODELPLUGIN plugins/md3model/mdlimage.cpp
g++ -o build/debug/plugins/md3model/md5.os $CFLAGS_MODELPLUGIN plugins/md3model/md5.cpp
g++ -o build/debug/md3model.so $LDFLAGS_DYNAMICLIB build/debug/plugins/md3model/plugin.os build/debug/plugins/md3model/mdl.os build/debug/plugins/md3model/md3.os build/debug/plugins/md3model/md2.os build/debug/plugins/md3model/mdc.os build/debug/plugins/md3model/mdlimage.os build/debug/plugins/md3model/md5.os -Lbuild/debug -L.
g++ -o build/debug/plugins/model/plugin.os $CFLAGS_MODELPLUGIN plugins/model/plugin.cpp
g++ -o build/debug/plugins/model/model.os $CFLAGS_MODELPLUGIN plugins/model/model.cpp
g++ -o build/debug/model.so $LDFLAGS_DYNAMICLIB build/debug/plugins/model/plugin.os build/debug/plugins/model/model.os -Lbuild/debug/libs -Llibs -lmathlib -lpicomodel
g++ -o build/debug/plugins/shaders/plugin.os $CFLAGS_VFSPK3 plugins/shaders/plugin.cpp
g++ -o build/debug/plugins/shaders/shaders.os $CFLAGS_VFSPK3 plugins/shaders/shaders.cpp
g++ -o build/debug/plugins/vfspk3/archive.os $CFLAGS_VFSPK3 plugins/vfspk3/archive.cpp
g++ -o build/debug/plugins/vfspk3/vfs.os $CFLAGS_VFSPK3 plugins/vfspk3/vfs.cpp
g++ -o build/debug/plugins/vfspk3/vfspk3.os $CFLAGS_VFSPK3 plugins/vfspk3/vfspk3.cpp
g++ -o build/debug/prtview.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` build/debug/contrib/prtview/AboutDialog.os build/debug/contrib/prtview/ConfigDialog.os build/debug/contrib/prtview/LoadPortalFileDialog.os build/debug/contrib/prtview/portals.os build/debug/contrib/prtview/prtview.os -Lbuild/debug/libs -Llibs -lprofile
gcc -o build/debug/tools/quake3/common/aselib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/aselib.c
gcc -o build/debug/tools/quake3/common/bspfile.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/bspfile.c
gcc -o build/debug/tools/quake3/common/cmdlib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/cmdlib.c
gcc -o build/debug/tools/quake3/common/imagelib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/imagelib.c
gcc -o build/debug/tools/quake3/common/inout.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/inout.c
gcc -o build/debug/tools/quake3/common/md4.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/md4.c
gcc -o build/debug/tools/quake3/common/scriplib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/scriplib.c
gcc -o build/debug/tools/quake3/common/trilib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/trilib.c
gcc -o build/debug/tools/quake3/common/unzip.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/unzip.c
gcc -o build/debug/tools/quake3/common/vfs.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/vfs.c
gcc -o build/debug/tools/quake3/q3data/3dslib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/3dslib.c
gcc -o build/debug/tools/quake3/q3data/compress.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/compress.c
gcc -o build/debug/tools/quake3/q3data/images.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/images.c
gcc -o build/debug/tools/quake3/q3data/md3lib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/md3lib.c
gcc -o build/debug/tools/quake3/q3data/models.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/models.c
gcc -o build/debug/tools/quake3/q3data/p3dlib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/p3dlib.c
gcc -o build/debug/tools/quake3/q3data/polyset.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/polyset.c
gcc -o build/debug/tools/quake3/q3data/q3data.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/q3data.c
gcc -o build/debug/tools/quake3/q3data/stripper.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/stripper.c
gcc -o build/debug/tools/quake3/q3data/video.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3data/video.c
g++ -o build/debug/q3data.x86 -fPIC -Wl,-fini,fini_stub -L. -static-libgcc `xml2-config --libs` `pkg-config glib-2.0 --libs` `libpng-config --ldflags` -lmhash -lpthread build/debug/tools/quake3/common/aselib.o build/debug/tools/quake3/common/bspfile.o build/debug/tools/quake3/common/cmdlib.o build/debug/tools/quake3/common/imagelib.o build/debug/tools/quake3/common/inout.o build/debug/tools/quake3/common/md4.o build/debug/tools/quake3/common/scriplib.o build/debug/tools/quake3/common/trilib.o build/debug/tools/quake3/common/unzip.o build/debug/tools/quake3/common/vfs.o build/debug/tools/quake3/q3data/3dslib.o build/debug/tools/quake3/q3data/compress.o build/debug/tools/quake3/q3data/images.o build/debug/tools/quake3/q3data/md3lib.o build/debug/tools/quake3/q3data/models.o build/debug/tools/quake3/q3data/p3dlib.o build/debug/tools/quake3/q3data/polyset.o build/debug/tools/quake3/q3data/q3data.o build/debug/tools/quake3/q3data/stripper.o build/debug/tools/quake3/q3data/video.o -Lbuild/debug/libs -Llibs -lmathlib -ll_net
gcc -o build/debug/tools/quake3/common/mutex.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/mutex.c
gcc -o build/debug/tools/quake3/common/polylib.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/polylib.c
gcc -o build/debug/tools/quake3/common/threads.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/common/threads.c
gcc -o build/debug/tools/quake3/q3map2/brush.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/brush.c
gcc -o build/debug/tools/quake3/q3map2/brush_primit.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/brush_primit.c
gcc -o build/debug/tools/quake3/q3map2/bsp.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/bsp.c
gcc -o build/debug/tools/quake3/q3map2/facebsp.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/facebsp.c
gcc -o build/debug/tools/quake3/q3map2/fog.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/fog.c
gcc -o build/debug/tools/quake3/q3map2/leakfile.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/leakfile.c
gcc -o build/debug/tools/quake3/q3map2/map.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/map.c
gcc -o build/debug/tools/quake3/q3map2/model.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/model.c
gcc -o build/debug/tools/quake3/q3map2/patch.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/patch.c
gcc -o build/debug/tools/quake3/q3map2/portals.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/portals.c
gcc -o build/debug/tools/quake3/q3map2/prtfile.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/prtfile.c
gcc -o build/debug/tools/quake3/q3map2/surface.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/surface.c
gcc -o build/debug/tools/quake3/q3map2/surface_fur.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/surface_fur.c
gcc -o build/debug/tools/quake3/q3map2/surface_meta.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/surface_meta.c
gcc -o build/debug/tools/quake3/q3map2/tjunction.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/tjunction.c
gcc -o build/debug/tools/quake3/q3map2/tree.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/tree.c
gcc -o build/debug/tools/quake3/q3map2/writebsp.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/writebsp.c
gcc -o build/debug/tools/quake3/q3map2/image.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/image.c
gcc -o build/debug/tools/quake3/q3map2/light.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/light.c
gcc -o build/debug/tools/quake3/q3map2/light_bounce.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/light_bounce.c
gcc -o build/debug/tools/quake3/q3map2/light_trace.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/light_trace.c
gcc -o build/debug/tools/quake3/q3map2/light_ydnar.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/light_ydnar.c
gcc -o build/debug/tools/quake3/q3map2/lightmaps_ydnar.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/lightmaps_ydnar.c
gcc -o build/debug/tools/quake3/q3map2/vis.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/vis.c
gcc -o build/debug/tools/quake3/q3map2/visflow.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/visflow.c
gcc -o build/debug/tools/quake3/q3map2/bspfile_abstract.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/bspfile_abstract.c
gcc -o build/debug/tools/quake3/q3map2/bspfile_ibsp.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/bspfile_ibsp.c
gcc -o build/debug/tools/quake3/q3map2/bspfile_rbsp.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/bspfile_rbsp.c
gcc -o build/debug/tools/quake3/q3map2/decals.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/decals.c
gcc -o build/debug/tools/quake3/q3map2/main.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/main.c
gcc -o build/debug/tools/quake3/q3map2/mesh.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/mesh.c
gcc -o build/debug/tools/quake3/q3map2/path_init.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/path_init.c
gcc -o build/debug/tools/quake3/q3map2/shaders.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/shaders.c
gcc -o build/debug/tools/quake3/q3map2/surface_extra.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/surface_extra.c
gcc -o build/debug/tools/quake3/q3map2/surface_foliage.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/surface_foliage.c
gcc -o build/debug/tools/quake3/q3map2/convert_ase.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/convert_ase.c
gcc -o build/debug/tools/quake3/q3map2/convert_map.o $CFLAGS_QUAKE3_Q3MAP2 tools/quake3/q3map2/convert_map.c
g++ -o build/debug/q3map2.x86 -fPIC -Wl,-fini,fini_stub -L. -static-libgcc `xml2-config --libs` `pkg-config glib-2.0 --libs` `libpng-config --ldflags` -lmhash -lpthread build/debug/tools/quake3/common/cmdlib.o build/debug/tools/quake3/common/imagelib.o build/debug/tools/quake3/common/inout.o build/debug/tools/quake3/common/mutex.o build/debug/tools/quake3/common/polylib.o build/debug/tools/quake3/common/scriplib.o build/debug/tools/quake3/common/threads.o build/debug/tools/quake3/common/unzip.o build/debug/tools/quake3/common/vfs.o build/debug/tools/quake3/q3map2/brush.o build/debug/tools/quake3/q3map2/brush_primit.o build/debug/tools/quake3/q3map2/bsp.o build/debug/tools/quake3/q3map2/facebsp.o build/debug/tools/quake3/q3map2/fog.o build/debug/tools/quake3/q3map2/leakfile.o build/debug/tools/quake3/q3map2/map.o build/debug/tools/quake3/q3map2/model.o build/debug/tools/quake3/q3map2/patch.o build/debug/tools/quake3/q3map2/portals.o build/debug/tools/quake3/q3map2/prtfile.o build/debug/tools/quake3/q3map2/surface.o build/debug/tools/quake3/q3map2/surface_fur.o build/debug/tools/quake3/q3map2/surface_meta.o build/debug/tools/quake3/q3map2/tjunction.o build/debug/tools/quake3/q3map2/tree.o build/debug/tools/quake3/q3map2/writebsp.o build/debug/tools/quake3/q3map2/image.o build/debug/tools/quake3/q3map2/light.o build/debug/tools/quake3/q3map2/light_bounce.o build/debug/tools/quake3/q3map2/light_trace.o build/debug/tools/quake3/q3map2/light_ydnar.o build/debug/tools/quake3/q3map2/lightmaps_ydnar.o build/debug/tools/quake3/q3map2/vis.o build/debug/tools/quake3/q3map2/visflow.o build/debug/tools/quake3/q3map2/bspfile_abstract.o build/debug/tools/quake3/q3map2/bspfile_ibsp.o build/debug/tools/quake3/q3map2/bspfile_rbsp.o build/debug/tools/quake3/q3map2/decals.o build/debug/tools/quake3/q3map2/main.o build/debug/tools/quake3/q3map2/mesh.o build/debug/tools/quake3/q3map2/path_init.o build/debug/tools/quake3/q3map2/shaders.o build/debug/tools/quake3/q3map2/surface_extra.o build/debug/tools/quake3/q3map2/surface_foliage.o build/debug/tools/quake3/q3map2/convert_ase.o build/debug/tools/quake3/q3map2/convert_map.o -Lbuild/debug/libs -Llibs -lmathlib -ll_net -ljpeg6 -lpicomodel -lddslib
gcc -o build/debug/tools/quake2/common/bspfile.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/bspfile.c
gcc -o build/debug/tools/quake2/common/cmdlib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/cmdlib.c
gcc -o build/debug/tools/quake2/common/inout.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/inout.c
gcc -o build/debug/tools/quake2/common/l3dslib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/l3dslib.c
gcc -o build/debug/tools/quake2/common/lbmlib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/lbmlib.c
gcc -o build/debug/tools/quake2/common/mathlib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/mathlib.c
gcc -o build/debug/tools/quake2/common/md4.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/md4.c
gcc -o build/debug/tools/quake2/common/path_init.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/path_init.c
gcc -o build/debug/tools/quake2/common/polylib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/polylib.c
gcc -o build/debug/tools/quake2/common/scriplib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/scriplib.c
gcc -o build/debug/tools/quake2/common/threads.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/threads.c
gcc -o build/debug/tools/quake2/common/trilib.o $CFLAGS_QUAKE2_QDATA tools/quake2/common/trilib.c
gcc -o build/debug/tools/quake2/q2map/brushbsp.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/brushbsp.c
gcc -o build/debug/tools/quake2/q2map/csg.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/csg.c
gcc -o build/debug/tools/quake2/q2map/faces.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/faces.c
gcc -o build/debug/tools/quake2/q2map/flow.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/flow.c
gcc -o build/debug/tools/quake2/q2map/glfile.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/glfile.c
gcc -o build/debug/tools/quake2/q2map/leakfile.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/leakfile.c
gcc -o build/debug/tools/quake2/q2map/lightmap.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/lightmap.c
gcc -o build/debug/tools/quake2/q2map/main.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/main.c
gcc -o build/debug/tools/quake2/q2map/map.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/map.c
gcc -o build/debug/tools/quake2/q2map/nodraw.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/nodraw.c
gcc -o build/debug/tools/quake2/q2map/patches.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/patches.c
gcc -o build/debug/tools/quake2/q2map/portals.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/portals.c
gcc -o build/debug/tools/quake2/q2map/prtfile.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/prtfile.c
gcc -o build/debug/tools/quake2/q2map/qbsp.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/qbsp.c
gcc -o build/debug/tools/quake2/q2map/qrad.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/qrad.c
gcc -o build/debug/tools/quake2/q2map/qvis.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/qvis.c
gcc -o build/debug/tools/quake2/q2map/textures.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/textures.c
gcc -o build/debug/tools/quake2/q2map/trace.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/trace.c
gcc -o build/debug/tools/quake2/q2map/tree.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/tree.c
gcc -o build/debug/tools/quake2/q2map/writebsp.o $CFLAGS_QUAKE2_QDATA tools/quake2/q2map/writebsp.c
g++ -o build/debug/quake2_tools/q2map -fPIC -Wl,-fini,fini_stub -L. -static-libgcc `xml2-config --libs` -lpthread build/debug/tools/quake2/common/bspfile.o build/debug/tools/quake2/common/cmdlib.o build/debug/tools/quake2/common/inout.o build/debug/tools/quake2/common/l3dslib.o build/debug/tools/quake2/common/lbmlib.o build/debug/tools/quake2/common/mathlib.o build/debug/tools/quake2/common/md4.o build/debug/tools/quake2/common/path_init.o build/debug/tools/quake2/common/polylib.o build/debug/tools/quake2/common/scriplib.o build/debug/tools/quake2/common/threads.o build/debug/tools/quake2/common/trilib.o build/debug/tools/quake2/q2map/brushbsp.o build/debug/tools/quake2/q2map/csg.o build/debug/tools/quake2/q2map/faces.o build/debug/tools/quake2/q2map/flow.o build/debug/tools/quake2/q2map/glfile.o build/debug/tools/quake2/q2map/leakfile.o build/debug/tools/quake2/q2map/lightmap.o build/debug/tools/quake2/q2map/main.o build/debug/tools/quake2/q2map/map.o build/debug/tools/quake2/q2map/nodraw.o build/debug/tools/quake2/q2map/patches.o build/debug/tools/quake2/q2map/portals.o build/debug/tools/quake2/q2map/prtfile.o build/debug/tools/quake2/q2map/qbsp.o build/debug/tools/quake2/q2map/qrad.o build/debug/tools/quake2/q2map/qvis.o build/debug/tools/quake2/q2map/textures.o build/debug/tools/quake2/q2map/trace.o build/debug/tools/quake2/q2map/tree.o build/debug/tools/quake2/q2map/writebsp.o -Lbuild/debug/libs -Llibs -ll_net
gcc -o build/debug/tools/quake2/qdata/images.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/images.c
gcc -o build/debug/tools/quake2/qdata/models.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/models.c
gcc -o build/debug/tools/quake2/qdata/qdata.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/qdata.c
gcc -o build/debug/tools/quake2/qdata/sprites.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/sprites.c
gcc -o build/debug/tools/quake2/qdata/tables.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/tables.c
gcc -o build/debug/tools/quake2/qdata/video.o $CFLAGS_QUAKE2_QDATA tools/quake2/qdata/video.c
g++ -o build/debug/quake2_tools/qdata3 -fPIC -Wl,-fini,fini_stub -L. -static-libgcc `xml2-config --libs` -lpthread build/debug/tools/quake2/common/bspfile.o build/debug/tools/quake2/common/cmdlib.o build/debug/tools/quake2/common/inout.o build/debug/tools/quake2/common/l3dslib.o build/debug/tools/quake2/common/lbmlib.o build/debug/tools/quake2/common/mathlib.o build/debug/tools/quake2/common/md4.o build/debug/tools/quake2/common/path_init.o build/debug/tools/quake2/common/polylib.o build/debug/tools/quake2/common/scriplib.o build/debug/tools/quake2/common/threads.o build/debug/tools/quake2/common/trilib.o build/debug/tools/quake2/qdata/images.o build/debug/tools/quake2/qdata/models.o build/debug/tools/quake2/qdata/qdata.o build/debug/tools/quake2/qdata/sprites.o build/debug/tools/quake2/qdata/tables.o build/debug/tools/quake2/qdata/video.o -Lbuild/debug/libs -Llibs -ll_net
g++ -o build/debug/radiant/autosave.o $CFLAGS_RADIANT radiant/autosave.cpp
g++ -o build/debug/radiant/brush.o $CFLAGS_RADIANT radiant/brush.cpp
g++ -o build/debug/radiant/brush_primit.o $CFLAGS_RADIANT radiant/brush_primit.cpp
g++ -o build/debug/radiant/brushmanip.o $CFLAGS_RADIANT radiant/brushmanip.cpp
g++ -o build/debug/radiant/brushmodule.o $CFLAGS_RADIANT radiant/brushmodule.cpp
g++ -o build/debug/radiant/brushnode.o $CFLAGS_RADIANT radiant/brushnode.cpp
g++ -o build/debug/radiant/brushtokens.o $CFLAGS_RADIANT radiant/brushtokens.cpp
g++ -o build/debug/radiant/brushxml.o $CFLAGS_RADIANT radiant/brushxml.cpp
g++ -o build/debug/radiant/build.o $CFLAGS_RADIANT radiant/build.cpp
g++ -o build/debug/radiant/camwindow.o $CFLAGS_RADIANT radiant/camwindow.cpp
g++ -o build/debug/radiant/clippertool.o $CFLAGS_RADIANT radiant/clippertool.cpp
g++ -o build/debug/radiant/commands.o $CFLAGS_RADIANT radiant/commands.cpp
g++ -o build/debug/radiant/console.o $CFLAGS_RADIANT radiant/console.cpp
g++ -o build/debug/radiant/csg.o $CFLAGS_RADIANT radiant/csg.cpp
g++ -o build/debug/radiant/dialog.o $CFLAGS_RADIANT radiant/dialog.cpp
g++ -o build/debug/radiant/eclass.o $CFLAGS_RADIANT radiant/eclass.cpp
g++ -o build/debug/radiant/eclass_def.o $CFLAGS_RADIANT radiant/eclass_def.cpp
g++ -o build/debug/radiant/eclass_doom3.o $CFLAGS_RADIANT radiant/eclass_doom3.cpp
g++ -o build/debug/radiant/eclass_fgd.o $CFLAGS_RADIANT radiant/eclass_fgd.cpp
g++ -o build/debug/radiant/eclass_xml.o $CFLAGS_RADIANT radiant/eclass_xml.cpp
g++ -o build/debug/radiant/entity.o $CFLAGS_RADIANT radiant/entity.cpp
g++ -o build/debug/radiant/entityinspector.o $CFLAGS_RADIANT radiant/entityinspector.cpp
g++ -o build/debug/radiant/entitylist.o $CFLAGS_RADIANT radiant/entitylist.cpp
g++ -o build/debug/radiant/environment.o $CFLAGS_RADIANT radiant/environment.cpp
g++ -o build/debug/radiant/error.o $CFLAGS_RADIANT radiant/error.cpp
g++ -o build/debug/radiant/feedback.o $CFLAGS_RADIANT radiant/feedback.cpp
g++ -o build/debug/radiant/filetypes.o $CFLAGS_RADIANT radiant/filetypes.cpp
g++ -o build/debug/radiant/filters.o $CFLAGS_RADIANT radiant/filters.cpp
g++ -o build/debug/radiant/findtexturedialog.o $CFLAGS_RADIANT radiant/findtexturedialog.cpp
g++ -o build/debug/radiant/glwidget.o $CFLAGS_RADIANT radiant/glwidget.cpp
g++ -o build/debug/radiant/grid.o $CFLAGS_RADIANT radiant/grid.cpp
g++ -o build/debug/radiant/groupdialog.o $CFLAGS_RADIANT radiant/groupdialog.cpp
g++ -o build/debug/radiant/gtkdlgs.o $CFLAGS_RADIANT radiant/gtkdlgs.cpp
g++ -o build/debug/radiant/gtkmisc.o $CFLAGS_RADIANT radiant/gtkmisc.cpp
g++ -o build/debug/radiant/help.o $CFLAGS_RADIANT radiant/help.cpp
g++ -o build/debug/radiant/image.o $CFLAGS_RADIANT radiant/image.cpp
g++ -o build/debug/radiant/main.o $CFLAGS_RADIANT radiant/main.cpp
g++ -o build/debug/radiant/mainframe.o $CFLAGS_RADIANT radiant/mainframe.cpp
g++ -o build/debug/radiant/map.o $CFLAGS_RADIANT radiant/map.cpp
g++ -o build/debug/radiant/mru.o $CFLAGS_RADIANT radiant/mru.cpp
g++ -o build/debug/radiant/nullmodel.o $CFLAGS_RADIANT radiant/nullmodel.cpp
g++ -o build/debug/radiant/parse.o $CFLAGS_RADIANT radiant/parse.cpp
g++ -o build/debug/radiant/patch.o $CFLAGS_RADIANT radiant/patch.cpp
g++ -o build/debug/radiant/patchdialog.o $CFLAGS_RADIANT radiant/patchdialog.cpp
g++ -o build/debug/radiant/patchmanip.o $CFLAGS_RADIANT radiant/patchmanip.cpp
g++ -o build/debug/radiant/patchmodule.o $CFLAGS_RADIANT radiant/patchmodule.cpp
g++ -o build/debug/radiant/plugin.o $CFLAGS_RADIANT radiant/plugin.cpp
g++ -o build/debug/radiant/pluginapi.o $CFLAGS_RADIANT radiant/pluginapi.cpp
g++ -o build/debug/radiant/pluginmanager.o $CFLAGS_RADIANT radiant/pluginmanager.cpp
g++ -o build/debug/radiant/pluginmenu.o $CFLAGS_RADIANT radiant/pluginmenu.cpp
g++ -o build/debug/radiant/plugintoolbar.o $CFLAGS_RADIANT radiant/plugintoolbar.cpp
g++ -o build/debug/radiant/points.o $CFLAGS_RADIANT radiant/points.cpp
g++ -o build/debug/radiant/preferencedictionary.o $CFLAGS_RADIANT radiant/preferencedictionary.cpp
g++ -o build/debug/radiant/preferences.o $CFLAGS_RADIANT radiant/preferences.cpp
g++ -o build/debug/radiant/qe3.o $CFLAGS_RADIANT radiant/qe3.cpp
g++ -o build/debug/radiant/qgl.o $CFLAGS_RADIANT radiant/qgl.cpp
g++ -o build/debug/radiant/referencecache.o $CFLAGS_RADIANT radiant/referencecache.cpp
g++ -o build/debug/radiant/renderer.o $CFLAGS_RADIANT radiant/renderer.cpp
g++ -o build/debug/radiant/renderstate.o $CFLAGS_RADIANT radiant/renderstate.cpp
g++ -o build/debug/radiant/scenegraph.o $CFLAGS_RADIANT radiant/scenegraph.cpp
g++ -o build/debug/radiant/select.o $CFLAGS_RADIANT radiant/select.cpp
g++ -o build/debug/radiant/selection.o $CFLAGS_RADIANT radiant/selection.cpp
g++ -o build/debug/radiant/server.o $CFLAGS_RADIANT radiant/server.cpp
g++ -o build/debug/radiant/shaders.o $CFLAGS_RADIANT radiant/shaders.cpp
g++ -o build/debug/radiant/sockets.o $CFLAGS_RADIANT radiant/sockets.cpp
g++ -o build/debug/radiant/stacktrace.o $CFLAGS_RADIANT radiant/stacktrace.cpp
g++ -o build/debug/radiant/surfacedialog.o $CFLAGS_RADIANT radiant/surfacedialog.cpp
g++ -o build/debug/radiant/texmanip.o $CFLAGS_RADIANT radiant/texmanip.cpp
g++ -o build/debug/radiant/textures.o $CFLAGS_RADIANT radiant/textures.cpp
g++ -o build/debug/radiant/texwindow.o $CFLAGS_RADIANT radiant/texwindow.cpp
g++ -o build/debug/radiant/timer.o $CFLAGS_RADIANT radiant/timer.cpp
g++ -o build/debug/radiant/treemodel.o $CFLAGS_RADIANT radiant/treemodel.cpp
g++ -o build/debug/radiant/undo.o $CFLAGS_RADIANT radiant/undo.cpp
g++ -o build/debug/radiant/url.o $CFLAGS_RADIANT radiant/url.cpp
g++ -o build/debug/radiant/view.o $CFLAGS_RADIANT radiant/view.cpp
g++ -o build/debug/radiant/watchbsp.o $CFLAGS_RADIANT radiant/watchbsp.cpp
g++ -o build/debug/radiant/winding.o $CFLAGS_RADIANT radiant/winding.cpp
g++ -o build/debug/radiant/windowobservers.o $CFLAGS_RADIANT radiant/windowobservers.cpp
g++ -o build/debug/radiant/xmlstuff.o $CFLAGS_RADIANT radiant/xmlstuff.cpp
g++ -o build/debug/radiant/xywindow.o $CFLAGS_RADIANT radiant/xywindow.cpp
g++ -o build/debug/radiant.x86 -fPIC -Wl,-fini,fini_stub -L. -static-libgcc -ldl -lGL `pkg-config glib-2.0 --libs` `xml2-config --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` -lgtkglext-x11-1.0 -lgdkglext-x11-1.0 build/debug/radiant/autosave.o build/debug/radiant/brush.o build/debug/radiant/brushmanip.o build/debug/radiant/brushmodule.o build/debug/radiant/brushnode.o build/debug/radiant/brushtokens.o build/debug/radiant/brushxml.o build/debug/radiant/brush_primit.o build/debug/radiant/build.o build/debug/radiant/camwindow.o build/debug/radiant/clippertool.o build/debug/radiant/commands.o build/debug/radiant/console.o build/debug/radiant/csg.o build/debug/radiant/dialog.o build/debug/radiant/eclass.o build/debug/radiant/eclass_def.o build/debug/radiant/eclass_doom3.o build/debug/radiant/eclass_fgd.o build/debug/radiant/eclass_xml.o build/debug/radiant/entity.o build/debug/radiant/entityinspector.o build/debug/radiant/entitylist.o build/debug/radiant/environment.o build/debug/radiant/error.o build/debug/radiant/feedback.o build/debug/radiant/filetypes.o build/debug/radiant/filters.o build/debug/radiant/findtexturedialog.o build/debug/radiant/glwidget.o build/debug/radiant/grid.o build/debug/radiant/groupdialog.o build/debug/radiant/gtkdlgs.o build/debug/radiant/gtkmisc.o build/debug/radiant/help.o build/debug/radiant/image.o build/debug/radiant/main.o build/debug/radiant/mainframe.o build/debug/radiant/map.o build/debug/radiant/mru.o build/debug/radiant/nullmodel.o build/debug/radiant/parse.o build/debug/radiant/patch.o build/debug/radiant/patchdialog.o build/debug/radiant/patchmanip.o build/debug/radiant/patchmodule.o build/debug/radiant/plugin.o build/debug/radiant/pluginapi.o build/debug/radiant/pluginmanager.o build/debug/radiant/pluginmenu.o build/debug/radiant/plugintoolbar.o build/debug/radiant/points.o build/debug/radiant/preferencedictionary.o build/debug/radiant/preferences.o build/debug/radiant/qe3.o build/debug/radiant/qgl.o build/debug/radiant/referencecache.o build/debug/radiant/renderer.o build/debug/radiant/renderstate.o build/debug/radiant/scenegraph.o build/debug/radiant/stacktrace.o build/debug/radiant/select.o build/debug/radiant/selection.o build/debug/radiant/server.o build/debug/radiant/shaders.o build/debug/radiant/sockets.o build/debug/radiant/surfacedialog.o build/debug/radiant/texmanip.o build/debug/radiant/textures.o build/debug/radiant/texwindow.o build/debug/radiant/timer.o build/debug/radiant/treemodel.o build/debug/radiant/undo.o build/debug/radiant/url.o build/debug/radiant/view.o build/debug/radiant/watchbsp.o build/debug/radiant/winding.o build/debug/radiant/windowobservers.o build/debug/radiant/xmlstuff.o build/debug/radiant/xywindow.o -Lbuild/debug/libs -Llibs -lmathlib -lcmdlib -ll_net -lprofile -lgtkutil -lxmllib
g++ -o build/debug/shaderplug.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` `xml2-config --libs` build/debug/contrib/shaderplug/shaderplug.os -Lbuild/debug/libs -Llibs -lxmllib
g++ -o build/debug/shaders.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` build/debug/plugins/shaders/shaders.os build/debug/plugins/shaders/plugin.os -Lbuild/debug/libs -Llibs -lcmdlib
g++ -o build/debug/sunplug.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` build/debug/contrib/sunplug/sunplug.os -Lbuild/debug/libs -Llibs
g++ -o build/debug/ufoaiplug.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` `pkg-config gtk+-2.0 --libs-only-L` `pkg-config gtk+-2.0 --libs-only-l` build/debug/contrib/ufoaiplug/ufoai.os build/debug/contrib/ufoaiplug/ufoai_filters.os build/debug/contrib/ufoaiplug/ufoai_gtk.os build/debug/contrib/ufoaiplug/ufoai_level.os -Lbuild/debug/libs -Llibs
g++ -o build/debug/vfspk3.so $LDFLAGS_DYNAMICLIB `pkg-config glib-2.0 --libs` build/debug/plugins/vfspk3/vfspk3.os build/debug/plugins/vfspk3/vfs.os build/debug/plugins/vfspk3/archive.os -Lbuild/debug -L.
mv "build/debug/h2data" "install/heretic2/h2data"
mv "build/debug/archivepak.so" "install/modules/archivepak.so"
mv "build/debug/archivewad.so" "install/modules/archivewad.so"
mv "build/debug/archivezip.so" "install/modules/archivezip.so"
mv "build/debug/entity.so" "install/modules/entity.so"
mv "build/debug/image.so" "install/modules/image.so"
mv "build/debug/imagehl.so" "install/modules/imagehl.so"
mv "build/debug/imagepng.so" "install/modules/imagepng.so"
mv "build/debug/imageq2.so" "install/modules/imageq2.so"
mv "build/debug/mapq3.so" "install/modules/mapq3.so"
mv "build/debug/mapxml.so" "install/modules/mapxml.so"
mv "build/debug/md3model.so" "install/modules/md3model.so"
mv "build/debug/model.so" "install/modules/model.so"
mv "build/debug/shaders.so" "install/modules/shaders.so"
mv "build/debug/vfspk3.so" "install/modules/vfspk3.so"
mv "build/debug/bobtoolz.so" "install/plugins/bobtoolz.so"
mv "build/debug/brushexport.so" "install/plugins/brushexport.so"
mv "build/debug/prtview.so" "install/plugins/prtview.so"
mv "build/debug/shaderplug.so" "install/plugins/shaderplug.so"
mv "build/debug/sunplug.so" "install/plugins/sunplug.so"
mv "build/debug/ufoaiplug.so" "install/plugins/ufoaiplug.so"
mv "build/debug/quake2_tools/q2map" "install/q2map"
## mv "build/debug/q3data.x86" "install/q3data.x86"
## mv "build/debug/q3map2.x86" "install/q3map2.x86"
mv "build/debug/quake2_tools/qdata3" "install/qdata3"
mv "build/debug/radiant.x86" "install/radiant.x86"

# TODO add install.sh's work
