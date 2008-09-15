CFLAGS = -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -g3 -fPIC
CXXFLAGS = $(CFLAGS) -Wno-non-virtual-dtor -Wreorder -fno-exceptions -fno-rtti
CPPFLAGS_COMMON = -DPOSIX -DXWINDOWS -D_DEBUG -D_LINUX
LDFLAGS_COMMON = 
EXE = x86
A = a
SO = so
NETAPI = berkley
LDFLAGS_SO = -fPIC -Wl,-fini,fini_stub -static-libgcc -ldl -shared

FIND ?= find
RANLIB ?= ranlib
AR ?= ar

CPPFLAGS_GLIB = `pkg-config glib-2.0 --cflags`
LDFLAGS_GLIB = `pkg-config glib-2.0 --libs`

CPPFLAGS_XML = `xml2-config --cflags`
LDFLAGS_XML = `xml2-config --libs`

CPPFLAGS_PNG = `libpng-config --cflags`
LDFLAGS_PNG = `libpng-config --libs`

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
all: install/q3map2.$(EXE)

.PHONY: clean
clean:
	$(FIND) . \( -name \*.o -o -name \*.$(SO) -o -name \*.$(A) -o -name \*.$(EXE) \) -exec $(RM) {} \;

%.$(EXE):
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

%.$(A):
	$(AR) rc $@ $^
	$(RANLIB) $@

install/q3map2.$(EXE): LIBS := -lmhash
install/q3map2.$(EXE): CPPFLAGS := $(CPPFLAGS_COMMON) $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_PNG) -Itools/quake3/common -Ilibs -Iinclude
install/q3map2.$(EXE): LDFLAGS := $(LDFLAGS_COMMON) $(LDFLAGS_XML) $(LDFLAGS_GLIB) $(LDFLAGS_PNG) 
install/q3map2.$(EXE): \
	tools/quake3/common/cmdlib.o \
	tools/quake3/common/imagelib.o \
	tools/quake3/common/inout.o \
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
	libmathlib.$(A) \
	libl_net.$(A) \
	libjpeg6.$(A) \
	libpicomodel.$(A) \
	libddslib.$(A) \

libmathlib.$(A): CPPFLAGS := $(CPPFLAGS_COMMON) -Ilibs
libmathlib.$(A): \
	libs/mathlib/mathlib.o \
	libs/mathlib/bbox.o \
	libs/mathlib/line.o \
	libs/mathlib/m4x4.o \
	libs/mathlib/ray.o \

libl_net.$(A): CPPFLAGS := $(CPPFLAGS_COMMON) -Ilibs
libl_net.$(A): \
	libs/l_net/l_net.o \
	libs/l_net/l_net_$(NETAPI).o \

libjpeg6.$(A): CPPFLAGS := $(CPPFLAGS_COMMON) -Ilibs/jpeg6 -Ilibs
libjpeg6.$(A): \
	libs/jpeg6/jcomapi.o \
	libs/jpeg6/jdcoefct.o \
	libs/jpeg6/jdinput.o \
	libs/jpeg6/jdpostct.o \
	libs/jpeg6/jfdctflt.o \
	libs/jpeg6/jpgload.o \
	libs/jpeg6/jdapimin.o \
	libs/jpeg6/jdcolor.o \
	libs/jpeg6/jdmainct.o \
	libs/jpeg6/jdsample.o \
	libs/jpeg6/jidctflt.o \
	libs/jpeg6/jutils.o \
	libs/jpeg6/jdapistd.o \
	libs/jpeg6/jddctmgr.o \
	libs/jpeg6/jdmarker.o \
	libs/jpeg6/jdtrans.o \
	libs/jpeg6/jmemmgr.o \
	libs/jpeg6/jdatasrc.o \
	libs/jpeg6/jdhuff.o \
	libs/jpeg6/jdmaster.o \
	libs/jpeg6/jerror.o \
	libs/jpeg6/jmemnobs.o \

libpicomodel.$(A): CPPFLAGS := $(CPPFLAGS_COMMON) -Ilibs
libpicomodel.$(A): \
	libs/picomodel/picointernal.o \
	libs/picomodel/picomodel.o \
	libs/picomodel/picomodules.o \
	libs/picomodel/pm_3ds.o \
	libs/picomodel/pm_ase.o \
	libs/picomodel/pm_md3.o \
	libs/picomodel/pm_obj.o \
	libs/picomodel/pm_ms3d.o \
	libs/picomodel/pm_mdc.o \
	libs/picomodel/pm_fm.o \
	libs/picomodel/pm_md2.o \
	libs/picomodel/pm_lwo.o \
	libs/picomodel/pm_terrain.o \
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

libddslib.$(A): CPPFLAGS := $(CPPFLAGS_COMMON) -Ilibs
libddslib.$(A): \
	libs/ddslib/ddslib.o \

