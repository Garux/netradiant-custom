include Makefile.conf

CFLAGS   = -MMD -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -fPIC
CPPFLAGS = -DPOSIX -DXWINDOWS -D_LINUX
CXXFLAGS = $(CFLAGS) -Wno-non-virtual-dtor -Wreorder -fno-exceptions -fno-rtti

CFLAGS_OPT ?= -O3

ifeq ($(BUILD),debug)
	CFLAGS += -g3
	CPPFLAGS += -D_DEBUG
else ifeq ($(BUILD),release)
	CFLAGS += $(CFLAGS_OPT)
	LDFLAGS += -s
else
	$(error Unsupported build type)
endif

ifeq ($(OS),Linux)
	LDFLAGS_DLL = -fPIC -ldl
	LIBS = -lpthread
	EXE = x86
	A = a
	DLL = so
	NETAPI = berkley
	CPPFLAGS_GLIB = `pkg-config glib-2.0 --cflags`
	LIBS_GLIB = `pkg-config glib-2.0 --libs-only-l --libs-only-L`
	CPPFLAGS_XML = `xml2-config --cflags`
	LIBS_XML = `xml2-config --libs`
	CPPFLAGS_PNG = `libpng-config --cflags`
	LIBS_PNG = `libpng-config --libs`
	CPPFLAGS_GTK = `pkg-config gtk+-2.0 --cflags`
	LIBS_GTK = `pkg-config gtk+-2.0 --libs-only-l --libs-only-L`
	CPPFLAGS_GTKGLEXT = `pkg-config gtkglext-1.0 --cflags`
	LIBS_GTKGLEXT = `pkg-config gtkglext-1.0 --libs-only-l --libs-only-L`
else ifeq ($(OS),Win32)
	$(error Unsupported build OS)
else ifeq ($(OS),Darwin)
	$(error Unsupported build OS)
else
	$(error Unsupported build OS)
endif

RADIANT_ABOUTMSG = Custom build

LDD ?= ldd
FIND ?= find
RANLIB ?= ranlib
AR ?= ar
MKDIR ?= mkdir -p
CP ?= cp
CP_R ?= $(CP) -r
RM_R ?= $(RM) -r
TEE_STDERR ?= | tee /dev/stderr

# from qe3.cpp: const char* const EXECUTABLE_TYPE = 
# from qe3.cpp: #if defined(__linux__) || defined (__FreeBSD__)
# from qe3.cpp: "x86"
# from qe3.cpp: #elif defined(__APPLE__)
# from qe3.cpp: "ppc"
# from qe3.cpp: #elif defined(WIN32)
# from qe3.cpp: "exe"
# from qe3.cpp: #else
# from qe3.cpp: #error "unknown platform"
# from qe3.cpp: #endif
# from qe3.cpp: ;

.PHONY: all
all: \
	makeversion \
	install/heretic2/h2data.$(EXE) \
	install/modules/archivepak.$(DLL) \
	install/modules/archivewad.$(DLL) \
	install/modules/archivezip.$(DLL) \
	install/modules/entity.$(DLL) \
	install/modules/image.$(DLL) \
	install/modules/imagehl.$(DLL) \
	install/modules/imagepng.$(DLL) \
	install/modules/imageq2.$(DLL) \
	install/modules/mapq3.$(DLL) \
	install/modules/mapxml.$(DLL) \
	install/modules/md3model.$(DLL) \
	install/modules/model.$(DLL) \
	install/modules/shaders.$(DLL) \
	install/modules/vfspk3.$(DLL) \
	install/plugins/bobtoolz.$(DLL) \
	install/plugins/brushexport.$(DLL) \
	install/plugins/prtview.$(DLL) \
	install/plugins/shaderplug.$(DLL) \
	install/plugins/sunplug.$(DLL) \
	install/plugins/ufoaiplug.$(DLL) \
	install/q2map.$(EXE) \
	install/q3data.$(EXE) \
	install/q3map2.$(EXE) \
	install/qdata3.$(EXE) \
	install/radiant.$(EXE) \
	install-data \

.PHONY: clean
clean:
	$(RM_R) install/
	$(FIND) . \( -name \*.o -o -name \*.d -o -name \*.$(DLL) -o -name \*.$(A) -o -name \*.$(EXE) \) -exec $(RM) {} \;

%.$(EXE):
	dir=$@; $(MKDIR) $${dir%/*}
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_EXTRA)
	[ -z "$(LDD)" ] || [ -z "`$(LDD) -r $@ 2>&1 >/dev/null $(TEE_STDERR)`" ] || { $(RM) $@; exit 1; }

%.$(A):
	$(AR) rc $@ $^
	$(RANLIB) $@

%.$(DLL):
	dir=$@; $(MKDIR) $${dir%/*}
	$(CXX) -shared -o $@ $^ $(LDFLAGS) $(LDFLAGS_DLL) $(LIBS) $(LIBS_EXTRA)
	[ -z "$(LDD)" ] || [ -z "`$(LDD) -r $@ 2>&1 >/dev/null $(TEE_STDERR)`" ] || { $(RM) $@; exit 1; }

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS) $(CPPFLAGS_EXTRA)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS) $(CPPFLAGS_EXTRA)

install/q3map2.$(EXE): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB) $(LIBS_PNG)
install/q3map2.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_PNG) -Itools/quake3/common -Ilibs -Iinclude
install/q3map2.$(EXE): \
	tools/quake3/common/cmdlib.o \
	tools/quake3/common/imagelib.o \
	tools/quake3/common/inout.o \
	tools/quake3/common/md4.o \
	tools/quake3/common/mutex.o \
	tools/quake3/common/polylib.o \
	tools/quake3/common/scriplib.o \
	tools/quake3/common/threads.o \
	tools/quake3/common/unzip.o \
	tools/quake3/common/vfs.o \
	tools/quake3/q3map2/brush.o \
	tools/quake3/q3map2/brush_primit.o \
	tools/quake3/q3map2/bspfile_abstract.o \
	tools/quake3/q3map2/bspfile_ibsp.o \
	tools/quake3/q3map2/bspfile_rbsp.o \
	tools/quake3/q3map2/bsp.o \
	tools/quake3/q3map2/convert_ase.o \
	tools/quake3/q3map2/convert_map.o \
	tools/quake3/q3map2/decals.o \
	tools/quake3/q3map2/facebsp.o \
	tools/quake3/q3map2/fog.o \
	tools/quake3/q3map2/image.o \
	tools/quake3/q3map2/leakfile.o \
	tools/quake3/q3map2/light_bounce.o \
	tools/quake3/q3map2/lightmaps_ydnar.o \
	tools/quake3/q3map2/light.o \
	tools/quake3/q3map2/light_trace.o \
	tools/quake3/q3map2/light_ydnar.o \
	tools/quake3/q3map2/main.o \
	tools/quake3/q3map2/map.o \
	tools/quake3/q3map2/mesh.o \
	tools/quake3/q3map2/model.o \
	tools/quake3/q3map2/patch.o \
	tools/quake3/q3map2/path_init.o \
	tools/quake3/q3map2/portals.o \
	tools/quake3/q3map2/prtfile.o \
	tools/quake3/q3map2/shaders.o \
	tools/quake3/q3map2/surface_extra.o \
	tools/quake3/q3map2/surface_foliage.o \
	tools/quake3/q3map2/surface_fur.o \
	tools/quake3/q3map2/surface_meta.o \
	tools/quake3/q3map2/surface.o \
	tools/quake3/q3map2/tjunction.o \
	tools/quake3/q3map2/tree.o \
	tools/quake3/q3map2/visflow.o \
	tools/quake3/q3map2/vis.o \
	tools/quake3/q3map2/writebsp.o \
	libddslib.$(A) \
	libjpeg6.$(A) \
	libl_net.$(A) \
	libmathlib.$(A) \
	libpicomodel.$(A) \

libmathlib.$(A): CPPFLAGS_EXTRA := -Ilibs
libmathlib.$(A): \
	libs/mathlib/bbox.o \
	libs/mathlib/line.o \
	libs/mathlib/m4x4.o \
	libs/mathlib/mathlib.o \
	libs/mathlib/ray.o \

libl_net.$(A): CPPFLAGS_EXTRA := -Ilibs
libl_net.$(A): \
	libs/l_net/l_net.o \
	libs/l_net/l_net_$(NETAPI).o \

libjpeg6.$(A): CPPFLAGS_EXTRA := -Ilibs/jpeg6 -Ilibs
libjpeg6.$(A): \
	libs/jpeg6/jcomapi.o \
	libs/jpeg6/jdapimin.o \
	libs/jpeg6/jdapistd.o \
	libs/jpeg6/jdatasrc.o \
	libs/jpeg6/jdcoefct.o \
	libs/jpeg6/jdcolor.o \
	libs/jpeg6/jddctmgr.o \
	libs/jpeg6/jdhuff.o \
	libs/jpeg6/jdinput.o \
	libs/jpeg6/jdmainct.o \
	libs/jpeg6/jdmarker.o \
	libs/jpeg6/jdmaster.o \
	libs/jpeg6/jdpostct.o \
	libs/jpeg6/jdsample.o \
	libs/jpeg6/jdtrans.o \
	libs/jpeg6/jerror.o \
	libs/jpeg6/jfdctflt.o \
	libs/jpeg6/jidctflt.o \
	libs/jpeg6/jmemmgr.o \
	libs/jpeg6/jmemnobs.o \
	libs/jpeg6/jpgload.o \
	libs/jpeg6/jutils.o \

libpicomodel.$(A): CPPFLAGS_EXTRA := -Ilibs
libpicomodel.$(A): \
	libs/picomodel/lwo/clip.o \
	libs/picomodel/lwo/envelope.o \
	libs/picomodel/lwo/list.o \
	libs/picomodel/lwo/lwio.o \
	libs/picomodel/lwo/lwo2.o \
	libs/picomodel/lwo/lwob.o \
	libs/picomodel/lwo/pntspols.o \
	libs/picomodel/lwo/surface.o \
	libs/picomodel/lwo/vecmath.o \
	libs/picomodel/lwo/vmap.o \
	libs/picomodel/picointernal.o \
	libs/picomodel/picomodel.o \
	libs/picomodel/picomodules.o \
	libs/picomodel/pm_3ds.o \
	libs/picomodel/pm_ase.o \
	libs/picomodel/pm_fm.o \
	libs/picomodel/pm_lwo.o \
	libs/picomodel/pm_md2.o \
	libs/picomodel/pm_md3.o \
	libs/picomodel/pm_mdc.o \
	libs/picomodel/pm_ms3d.o \
	libs/picomodel/pm_obj.o \
	libs/picomodel/pm_terrain.o \

libddslib.$(A): CPPFLAGS_EXTRA := -Ilibs
libddslib.$(A): \
	libs/ddslib/ddslib.o \

install/q3data.$(EXE): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB)
install/q3data.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) -Itools/quake3/common -Ilibs -Iinclude
install/q3data.$(EXE): \
	tools/quake3/common/aselib.o \
	tools/quake3/common/bspfile.o \
	tools/quake3/common/cmdlib.o \
	tools/quake3/common/imagelib.o \
	tools/quake3/common/inout.o \
	tools/quake3/common/md4.o \
	tools/quake3/common/scriplib.o \
	tools/quake3/common/trilib.o \
	tools/quake3/common/unzip.o \
	tools/quake3/common/vfs.o \
	tools/quake3/q3data/3dslib.o \
	tools/quake3/q3data/compress.o \
	tools/quake3/q3data/images.o \
	tools/quake3/q3data/md3lib.o \
	tools/quake3/q3data/models.o \
	tools/quake3/q3data/p3dlib.o \
	tools/quake3/q3data/polyset.o \
	tools/quake3/q3data/q3data.o \
	tools/quake3/q3data/stripper.o \
	tools/quake3/q3data/video.o \
	libl_net.$(A) \
	libmathlib.$(A) \

install/radiant.$(EXE): LIBS_EXTRA := -ldl -lGL -static-libgcc $(LIBS_XML) $(LIBS_GLIB) $(LIBS_GTK) $(LIBS_GTKGLEXT)
install/radiant.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_GTKGLEXT) -Ilibs -Iinclude
install/radiant.$(EXE): \
	radiant/autosave.o \
	radiant/brushmanip.o \
	radiant/brushmodule.o \
	radiant/brushnode.o \
	radiant/brush.o \
	radiant/brush_primit.o \
	radiant/brushtokens.o \
	radiant/brushxml.o \
	radiant/build.o \
	radiant/camwindow.o \
	radiant/clippertool.o \
	radiant/commands.o \
	radiant/console.o \
	radiant/csg.o \
	radiant/dialog.o \
	radiant/eclass_def.o \
	radiant/eclass_doom3.o \
	radiant/eclass_fgd.o \
	radiant/eclass.o \
	radiant/eclass_xml.o \
	radiant/entityinspector.o \
	radiant/entitylist.o \
	radiant/entity.o \
	radiant/environment.o \
	radiant/error.o \
	radiant/feedback.o \
	radiant/filetypes.o \
	radiant/filters.o \
	radiant/findtexturedialog.o \
	radiant/glwidget.o \
	radiant/grid.o \
	radiant/groupdialog.o \
	radiant/gtkdlgs.o \
	radiant/gtkmisc.o \
	radiant/help.o \
	radiant/image.o \
	radiant/mainframe.o \
	radiant/main.o \
	radiant/map.o \
	radiant/mru.o \
	radiant/nullmodel.o \
	radiant/parse.o \
	radiant/patchdialog.o \
	radiant/patchmanip.o \
	radiant/patchmodule.o \
	radiant/patch.o \
	radiant/pluginapi.o \
	radiant/pluginmanager.o \
	radiant/pluginmenu.o \
	radiant/plugin.o \
	radiant/plugintoolbar.o \
	radiant/points.o \
	radiant/preferencedictionary.o \
	radiant/preferences.o \
	radiant/qe3.o \
	radiant/qgl.o \
	radiant/referencecache.o \
	radiant/renderer.o \
	radiant/renderstate.o \
	radiant/scenegraph.o \
	radiant/selection.o \
	radiant/select.o \
	radiant/server.o \
	radiant/shaders.o \
	radiant/sockets.o \
	radiant/stacktrace.o \
	radiant/surfacedialog.o \
	radiant/texmanip.o \
	radiant/textures.o \
	radiant/texwindow.o \
	radiant/timer.o \
	radiant/treemodel.o \
	radiant/undo.o \
	radiant/url.o \
	radiant/view.o \
	radiant/watchbsp.o \
	radiant/winding.o \
	radiant/windowobservers.o \
	radiant/xmlstuff.o \
	radiant/xywindow.o \
	libcmdlib.$(A) \
	libgtkutil.$(A) \
	libl_net.$(A) \
	libmathlib.$(A) \
	libprofile.$(A) \
	libxmllib.$(A) \

libcmdlib.$(A): CPPFLAGS_EXTRA := -Ilibs
libcmdlib.$(A): \
	libs/cmdlib/cmdlib.o \

libprofile.$(A): CPPFLAGS_EXTRA := -Ilibs -Iinclude
libprofile.$(A): \
	libs/profile/file.o \
	libs/profile/profile.o \

libgtkutil.$(A): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_GTKGLEXT) -Ilibs -Iinclude
libgtkutil.$(A): \
	libs/gtkutil/accelerator.o \
	libs/gtkutil/button.o \
	libs/gtkutil/clipboard.o \
	libs/gtkutil/closure.o \
	libs/gtkutil/container.o \
	libs/gtkutil/cursor.o \
	libs/gtkutil/dialog.o \
	libs/gtkutil/entry.o \
	libs/gtkutil/filechooser.o \
	libs/gtkutil/frame.o \
	libs/gtkutil/glfont.o \
	libs/gtkutil/glwidget.o \
	libs/gtkutil/idledraw.o \
	libs/gtkutil/image.o \
	libs/gtkutil/menu.o \
	libs/gtkutil/messagebox.o \
	libs/gtkutil/nonmodal.o \
	libs/gtkutil/paned.o \
	libs/gtkutil/pointer.o \
	libs/gtkutil/toolbar.o \
	libs/gtkutil/widget.o \
	libs/gtkutil/window.o \
	libs/gtkutil/xorrectangle.o \

libxmllib.$(A): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) -Ilibs -Iinclude
libxmllib.$(A): \
	libs/xml/ixml.o \
	libs/xml/xmlelement.o \
	libs/xml/xmlparser.o \
	libs/xml/xmltextags.o \
	libs/xml/xmlwriter.o \

install/modules/archivezip.$(DLL): LIBS_EXTRA := -lz
install/modules/archivezip.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/archivezip.$(DLL): \
	plugins/archivezip/archive.o \
	plugins/archivezip/pkzip.o \
	plugins/archivezip/plugin.o \
	plugins/archivezip/zlibstream.o \

install/modules/archivewad.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/archivewad.$(DLL): \
	plugins/archivewad/archive.o \
	plugins/archivewad/plugin.o \
	plugins/archivewad/wad.o \

install/modules/archivepak.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/archivepak.$(DLL): \
	plugins/archivepak/archive.o \
	plugins/archivepak/pak.o \
	plugins/archivepak/plugin.o \

install/modules/entity.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/entity.$(DLL): \
	plugins/entity/angle.o \
	plugins/entity/angles.o \
	plugins/entity/colour.o \
	plugins/entity/doom3group.o \
	plugins/entity/eclassmodel.o \
	plugins/entity/entity.o \
	plugins/entity/filters.o \
	plugins/entity/generic.o \
	plugins/entity/group.o \
	plugins/entity/light.o \
	plugins/entity/miscmodel.o \
	plugins/entity/model.o \
	plugins/entity/modelskinkey.o \
	plugins/entity/namedentity.o \
	plugins/entity/origin.o \
	plugins/entity/plugin.o \
	plugins/entity/rotation.o \
	plugins/entity/scale.o \
	plugins/entity/skincache.o \
	plugins/entity/targetable.o \

install/modules/image.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/image.$(DLL): \
	plugins/image/bmp.o \
	plugins/image/dds.o \
	plugins/image/image.o \
	plugins/image/jpeg.o \
	plugins/image/pcx.o \
	plugins/image/tga.o \
	libddslib.$(A) \
	libjpeg6.$(A) \

install/modules/imageq2.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/imageq2.$(DLL): \
	plugins/imageq2/imageq2.o \
	plugins/imageq2/wal32.o \
	plugins/imageq2/wal.o \

install/modules/imagehl.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/imagehl.$(DLL): \
	plugins/imagehl/hlw.o \
	plugins/imagehl/imagehl.o \
	plugins/imagehl/mip.o \
	plugins/imagehl/sprite.o \

install/modules/imagepng.$(DLL): LIBS_EXTRA := $(LIBS_PNG)
install/modules/imagepng.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_PNG) -Ilibs -Iinclude
install/modules/imagepng.$(DLL): \
	plugins/imagepng/plugin.o \

install/modules/mapq3.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/mapq3.$(DLL): \
	plugins/mapq3/parse.o \
	plugins/mapq3/plugin.o \
	plugins/mapq3/write.o \

install/modules/mapxml.$(DLL): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB)
install/modules/mapxml.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) -Ilibs -Iinclude
install/modules/mapxml.$(DLL): \
	plugins/mapxml/plugin.o \
	plugins/mapxml/xmlparse.o \
	plugins/mapxml/xmlwrite.o \

install/modules/md3model.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/md3model.$(DLL): \
	plugins/md3model/md2.o \
	plugins/md3model/md3.o \
	plugins/md3model/md5.o \
	plugins/md3model/mdc.o \
	plugins/md3model/mdlimage.o \
	plugins/md3model/mdl.o \
	plugins/md3model/plugin.o \

install/modules/model.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
install/modules/model.$(DLL): \
	plugins/model/model.o \
	plugins/model/plugin.o \
	libpicomodel.$(A) \

install/modules/shaders.$(DLL): LIBS_EXTRA := $(LIBS_GLIB)
install/modules/shaders.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) -Ilibs -Iinclude
install/modules/shaders.$(DLL): \
	plugins/shaders/plugin.o \
	plugins/shaders/shaders.o \

install/modules/vfspk3.$(DLL): LIBS_EXTRA := $(LIBS_GLIB)
install/modules/vfspk3.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) -Ilibs -Iinclude
install/modules/vfspk3.$(DLL): \
	plugins/vfspk3/archive.o \
	plugins/vfspk3/vfs.o \
	plugins/vfspk3/vfspk3.o \

install/plugins/bobtoolz.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
install/plugins/bobtoolz.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
install/plugins/bobtoolz.$(DLL): \
	contrib/bobtoolz/bobToolz-GTK.o \
	contrib/bobtoolz/bsploader.o \
	contrib/bobtoolz/cportals.o \
	contrib/bobtoolz/DBobView.o \
	contrib/bobtoolz/DBrush.o \
	contrib/bobtoolz/DEntity.o \
	contrib/bobtoolz/DEPair.o \
	contrib/bobtoolz/dialogs/dialogs-gtk.o \
	contrib/bobtoolz/DMap.o \
	contrib/bobtoolz/DPatch.o \
	contrib/bobtoolz/DPlane.o \
	contrib/bobtoolz/DPoint.o \
	contrib/bobtoolz/DShape.o \
	contrib/bobtoolz/DTrainDrawer.o \
	contrib/bobtoolz/DTreePlanter.o \
	contrib/bobtoolz/DVisDrawer.o \
	contrib/bobtoolz/DWinding.o \
	contrib/bobtoolz/funchandlers-GTK.o \
	contrib/bobtoolz/lists.o \
	contrib/bobtoolz/misc.o \
	contrib/bobtoolz/ScriptParser.o \
	contrib/bobtoolz/shapes.o \
	contrib/bobtoolz/visfind.o \
	libcmdlib.$(A) \
	libmathlib.$(A) \
	libprofile.$(A) \

install/plugins/brushexport.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
install/plugins/brushexport.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
install/plugins/brushexport.$(DLL): \
	contrib/brushexport/callbacks.o \
	contrib/brushexport/export.o \
	contrib/brushexport/interface.o \
	contrib/brushexport/plugin.o \
	contrib/brushexport/support.o \

install/plugins/prtview.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
install/plugins/prtview.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
install/plugins/prtview.$(DLL): \
	contrib/prtview/AboutDialog.o \
	contrib/prtview/ConfigDialog.o \
	contrib/prtview/LoadPortalFileDialog.o \
	contrib/prtview/portals.o \
	contrib/prtview/prtview.o \
	libprofile.$(A) \

install/plugins/shaderplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK) $(LIBS_XML)
install/plugins/shaderplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_XML) -Ilibs -Iinclude
install/plugins/shaderplug.$(DLL): \
	contrib/shaderplug/shaderplug.o \
	libxmllib.$(A) \

install/plugins/sunplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
install/plugins/sunplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
install/plugins/sunplug.$(DLL): \
	contrib/sunplug/sunplug.o \

install/qdata3.$(EXE): LIBS_EXTRA := $(LIBS_XML)
install/qdata3.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/common -Ilibs -Iinclude
install/qdata3.$(EXE): \
	tools/quake2/common/bspfile.o \
	tools/quake2/common/cmdlib.o \
	tools/quake2/common/inout.o \
	tools/quake2/common/l3dslib.o \
	tools/quake2/common/lbmlib.o \
	tools/quake2/common/mathlib.o \
	tools/quake2/common/md4.o \
	tools/quake2/common/path_init.o \
	tools/quake2/common/polylib.o \
	tools/quake2/common/scriplib.o \
	tools/quake2/common/threads.o \
	tools/quake2/common/trilib.o \
	tools/quake2/qdata/images.o \
	tools/quake2/qdata/models.o \
	tools/quake2/qdata/qdata.o \
	tools/quake2/qdata/sprites.o \
	tools/quake2/qdata/tables.o \
	tools/quake2/qdata/video.o \
	libl_net.$(A) \

install/q2map.$(EXE): LIBS_EXTRA := $(LIBS_XML)
install/q2map.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/common -Ilibs -Iinclude
install/q2map.$(EXE): \
	tools/quake2/common/bspfile.o \
	tools/quake2/common/cmdlib.o \
	tools/quake2/common/inout.o \
	tools/quake2/common/l3dslib.o \
	tools/quake2/common/lbmlib.o \
	tools/quake2/common/mathlib.o \
	tools/quake2/common/md4.o \
	tools/quake2/common/path_init.o \
	tools/quake2/common/polylib.o \
	tools/quake2/common/scriplib.o \
	tools/quake2/common/threads.o \
	tools/quake2/common/trilib.o \
	tools/quake2/q2map/brushbsp.o \
	tools/quake2/q2map/csg.o \
	tools/quake2/q2map/faces.o \
	tools/quake2/q2map/flow.o \
	tools/quake2/q2map/glfile.o \
	tools/quake2/q2map/leakfile.o \
	tools/quake2/q2map/lightmap.o \
	tools/quake2/q2map/main.o \
	tools/quake2/q2map/map.o \
	tools/quake2/q2map/nodraw.o \
	tools/quake2/q2map/patches.o \
	tools/quake2/q2map/portals.o \
	tools/quake2/q2map/prtfile.o \
	tools/quake2/q2map/qbsp.o \
	tools/quake2/q2map/qrad.o \
	tools/quake2/q2map/qvis.o \
	tools/quake2/q2map/textures.o \
	tools/quake2/q2map/trace.o \
	tools/quake2/q2map/tree.o \
	tools/quake2/q2map/writebsp.o \
	libl_net.$(A) \

install/plugins/ufoaiplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
install/plugins/ufoaiplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
install/plugins/ufoaiplug.$(DLL): \
	contrib/ufoaiplug/ufoai_filters.o \
	contrib/ufoaiplug/ufoai_gtk.o \
	contrib/ufoaiplug/ufoai_level.o \
	contrib/ufoaiplug/ufoai.o \

install/heretic2/h2data.$(EXE): LIBS_EXTRA := $(LIBS_XML)
install/heretic2/h2data.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/qdata_heretic2/common -Itools/quake2/qdata_heretic2/qcommon -Itools/quake2/qdata_heretic2 -Itools/quake2/common -Ilibs -Iinclude
install/heretic2/h2data.$(EXE): \
	tools/quake2/qdata_heretic2/common/bspfile.o \
	tools/quake2/qdata_heretic2/common/cmdlib.o \
	tools/quake2/qdata_heretic2/common/inout.o \
	tools/quake2/qdata_heretic2/common/l3dslib.o \
	tools/quake2/qdata_heretic2/common/lbmlib.o \
	tools/quake2/qdata_heretic2/common/mathlib.o \
	tools/quake2/qdata_heretic2/common/md4.o \
	tools/quake2/qdata_heretic2/common/path_init.o \
	tools/quake2/qdata_heretic2/common/qfiles.o \
	tools/quake2/qdata_heretic2/common/scriplib.o \
	tools/quake2/qdata_heretic2/common/threads.o \
	tools/quake2/qdata_heretic2/common/token.o \
	tools/quake2/qdata_heretic2/common/trilib.o \
	tools/quake2/qdata_heretic2/qcommon/reference.o \
	tools/quake2/qdata_heretic2/qcommon/resourcemanager.o \
	tools/quake2/qdata_heretic2/qcommon/skeletons.o \
	tools/quake2/qdata_heretic2/animcomp.o \
	tools/quake2/qdata_heretic2/book.o \
	tools/quake2/qdata_heretic2/fmodels.o \
	tools/quake2/qdata_heretic2/images.o \
	tools/quake2/qdata_heretic2/jointed.o \
	tools/quake2/qdata_heretic2/models.o \
	tools/quake2/qdata_heretic2/pics.o \
	tools/quake2/qdata_heretic2/qdata.o \
	tools/quake2/qdata_heretic2/qd_skeletons.o \
	tools/quake2/qdata_heretic2/sprites.o \
	tools/quake2/qdata_heretic2/svdcmp.o \
	tools/quake2/qdata_heretic2/tables.o \
	tools/quake2/qdata_heretic2/tmix.o \
	tools/quake2/qdata_heretic2/video.o \
	libl_net.$(A) \

.PHONY: makeversion
makeversion:
	set -ex; \
	ver=`cat include/version.default`; \
	major=`echo $$ver | cut -d . -f 2`; \
	minor=`echo $$ver | cut -d . -f 3 | cut -d - -f 1`; \
	echo "// generated header, see Makefile" > include/version.h; \
	echo "#define RADIANT_VERSION \"$$ver\"" >> include/version.h; \
	echo "#define RADIANT_MAJOR_VERSION \"$$major\"" >> include/version.h; \
	echo "#define RADIANT_MINOR_VERSION \"$$minor\"" >> include/version.h; \
	echo "$$major" > include/RADIANT_MAJOR; \
	echo "$$minor" > include/RADIANT_MINOR; \
	echo "$$ver" > include/version; \
	echo "// generated header, see Makefile" > include/aboutmsg.h; \
	echo "#define RADIANT_ABOUTMSG \"$(RADIANT_ABOUTMSG)\"" >> include/aboutmsg.h; \

.PHONY: install-data
install-data:
	$(MKDIR) install/games
	set -ex; \
	for GAME in games/*; do \
		for GAMEFILE in $$GAME/games/*.game; do \
			$(CP) "$$GAMEFILE" install/games/; \
		done; \
		for GAMEDIR in $$GAME/*.game; do \
			$(CP_R) "$$GAMEDIR" install/; \
		done; \
	done
	$(CP) include/RADIANT_MAJOR install/
	$(CP) include/RADIANT_MINOR install/
	$(CP_R) setup/data/tools/* install/
	$(FIND) install/ -name .svn -exec $(RM_R) {} \; -prune; \

-include $(shell find . -name \*.d)
