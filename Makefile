MAKEFILE_CONF      ?= Makefile.conf
-include $(MAKEFILE_CONF)

## CONFIGURATION SETTINGS
# user customizable stuf
# you may override this in Makefile.conf or the environment
BUILD              ?= debug
# or: release, or: extradebug, or: profile
OS                 ?= $(shell uname)
# or: Linux, Win32, Darwin
LDFLAGS            ?=
CFLAGS             ?=
CXXFLAGS           ?=
CPPFLAGS           ?=
LIBS               ?=
RADIANT_ABOUTMSG   ?= Custom build

# warning: this directory may NOT contain any files other than the ones written by this Makefile!
# NEVER SET THIS TO A SYSTEM WIDE "bin" DIRECTORY!
INSTALLDIR         ?= install

CC                 ?= gcc
CXX                ?= g++
RANLIB             ?= ranlib
AR                 ?= ar
LDD                ?= ldd # nothing on Win32
OTOOL              ?= # only used on OS X
WINDRES            ?= windres # only used on Win32

PKGCONFIG          ?= pkg-config
PKG_CONFIG_PATH    ?=

SH                 ?= $(SHELL)
ECHO               ?= echo
ECHO_NOLF          ?= echo -n
CAT                ?= cat
MKDIR              ?= mkdir -p
CP                 ?= cp
CP_R               ?= $(CP) -r
LN                 ?= ln
LN_SNF             ?= $(LN) -snf
RM                 ?= rm
RM_R               ?= $(RM) -r
TEE_STDERR         ?= | tee /dev/stderr
TR                 ?= tr
FIND               ?= find
DIFF               ?= diff
SED                ?= sed
GIT                ?= git
SVN                ?= svn
WGET               ?= wget
MV                 ?= mv
UNZIPPER           ?= unzip

FD_TO_DEVNULL      ?= >/dev/null
STDOUT_TO_DEVNULL  ?= 1$(FD_TO_DEVNULL)
STDERR_TO_DEVNULL  ?= 2$(FD_TO_DEVNULL)
STDERR_TO_STDOUT   ?= 2>&1
TO_DEVNULL         ?= $(STDOUT_TO_DEVNULL) $(STDERR_TO_STDOUT)

CPPFLAGS_GLIB      ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) glib-2.0 --cflags $(STDERR_TO_DEVNULL))
LIBS_GLIB          ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) glib-2.0 --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) glib-2.0 --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_XML       ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libxml-2.0 --cflags $(STDERR_TO_DEVNULL))
LIBS_XML           ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libxml-2.0 --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libxml-2.0 --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_PNG       ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libpng --cflags $(STDERR_TO_DEVNULL))
LIBS_PNG           ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libpng --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) libpng --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_GTK       ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtk+-2.0 --cflags $(STDERR_TO_DEVNULL))
LIBS_GTK           ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtk+-2.0 --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtk+-2.0 --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_PANGOFT2  ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) pangoft2 --cflags $(STDERR_TO_DEVNULL))
LIBS_PANGOFT2      ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) pangoft2 --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) pangoft2 --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_GTKGLEXT  ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtkglext-1.0 --cflags $(STDERR_TO_DEVNULL))
LIBS_GTKGLEXT      ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtkglext-1.0 --libs-only-L $(STDERR_TO_DEVNULL)) \
                      $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtkglext-1.0 --libs-only-l $(STDERR_TO_DEVNULL))
CPPFLAGS_GL        ?=
LIBS_GL            ?= -lGL # -lopengl32 on Win32
CPPFLAGS_DL        ?=
LIBS_DL            ?= -ldl # nothing on Win32
CPPFLAGS_ZLIB      ?=
LIBS_ZLIB          ?= -lz
CPPFLAGS_JPEG      ?=
LIBS_JPEG          ?= -ljpeg
DEPEND_ON_MAKEFILE ?= yes
DOWNLOAD_GAMEPACKS ?= yes
# set to no to disable gamepack, set to all to even download undistributable gamepacks

# Support CHECK_DEPENDENCIES with DOWNLOAD_GAMEPACKS semantics
ifneq ($(CHECK_DEPENDENCIES),)
DEPENDENCIES_CHECK = $(patsubst yes,quiet,$(patsubst no,off,$(CHECK_DEPENDENCIES)))
else
DEPENDENCIES_CHECK ?= quiet
# or: off, verbose
endif

# these are used on Win32 only
GTKDIR             ?= $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) $(PKGCONFIG) gtk+-2.0 --variable=prefix $(STDERR_TO_DEVNULL))
WHICHDLL           ?= which
DLLINSTALL         ?= install-dlls.sh

# alias mingw32 OSes
ifeq ($(OS),MINGW32_NT-6.0)
	OS = Win32
endif
ifeq ($(OS),Windows_NT)
	OS = Win32
endif

CFLAGS_COMMON = -MMD -W -Wall -Wcast-align -Wcast-qual -Wno-unused-parameter -fno-strict-aliasing
CPPFLAGS_COMMON =
LDFLAGS_COMMON =
LIBS_COMMON =
CXXFLAGS_COMMON = -Wno-non-virtual-dtor -Wreorder -fno-exceptions -fno-rtti

ifeq ($(BUILD),debug)
ifeq ($(findstring $(CFLAGS),-g),)
	CFLAGS_COMMON += -g
	# only add -g if no -g flag is in $(CFLAGS)
endif
ifeq ($(findstring $(CFLAGS),-O),)
	CFLAGS_COMMON += -O
	# only add -O if no -O flag is in $(CFLAGS)
endif
	CPPFLAGS_COMMON +=
	LDFLAGS_COMMON +=
else

ifeq ($(BUILD),extradebug)
ifeq ($(findstring $(CFLAGS),-g),)
	CFLAGS_COMMON += -g3
	# only add -g3 if no -g flag is in $(CFLAGS)
endif
	CPPFLAGS_COMMON += -D_DEBUG
	LDFLAGS_COMMON +=
else

ifeq ($(BUILD),profile)
ifeq ($(findstring $(CFLAGS),-g),)
	CFLAGS_COMMON += -g
	# only add -g if no -g flag is in $(CFLAGS)
endif
ifeq ($(findstring $(CFLAGS),-O),)
	CFLAGS_COMMON += -O
	# only add -O if no -O flag is in $(CFLAGS)
endif
	CFLAGS_COMMON += -pg
	CPPFLAGS_COMMON +=
	LDFLAGS_COMMON += -pg
else

ifeq ($(BUILD),release)
ifeq ($(findstring $(CFLAGS),-O),)
	CFLAGS_COMMON += -O3
	# only add -O3 if no -O flag is in $(CFLAGS)
endif
	CPPFLAGS_COMMON +=
	LDFLAGS_COMMON += -s
else

ifeq ($(BUILD),native)
ifeq ($(findstring $(CFLAGS),-O),)
	CFLAGS_COMMON += -O3
	# only add -O3 if no -O flag is in $(CFLAGS)
endif
	CFLAGS_COMMON += -march=native -mcpu=native
	CPPFLAGS_COMMON +=
	LDFLAGS_COMMON += -s
else

$(error Unsupported build type: $(BUILD))
endif
endif
endif
endif
endif

INSTALLDIR_BASE := $(INSTALLDIR)

MAKE_EXE_SYMLINK = false

ifeq ($(OS),Linux)
	CPPFLAGS_COMMON += -DPOSIX -DXWINDOWS
	CFLAGS_COMMON += -fPIC
	LDFLAGS_DLL = -fPIC -ldl
	LIBS_COMMON = -lpthread
	EXE ?= $(shell uname -m)
	MAKE_EXE_SYMLINK = true
	A = a
	DLL = so
	MWINDOWS =
else

ifeq ($(OS),Win32)
	CPPFLAGS_COMMON += -DWIN32 -D_WIN32 -D_inline=inline
	CFLAGS_COMMON += -mms-bitfields
	LDFLAGS_DLL = -Wl,--add-stdcall-alias
	LIBS_COMMON = -lws2_32 -luser32 -lgdi32 -lole32
	EXE ?= exe
	A = a
	DLL = dll
	MWINDOWS = -mwindows

	# workaround: we have no "ldd" for Win32, so...
	LDD =
	# workaround: OpenGL library for Win32 is called opengl32.dll
	LIBS_GL = -lopengl32
	# workaround: no -ldl on Win32
	LIBS_DL = 
else

ifeq ($(OS),Darwin)
	CPPFLAGS_COMMON += -DPOSIX -DXWINDOWS
	CFLAGS_COMMON += -fPIC
	CXXFLAGS_COMMON += -fno-exceptions -fno-rtti
	MACLIBDIR ?= /opt/local/lib
	CPPFLAGS_COMMON += -I$(MACLIBDIR)/../include -I/usr/X11R6/include
	LDFLAGS_COMMON += -L$(MACLIBDIR) -L/usr/X11R6/lib
	LDFLAGS_DLL += -dynamiclib -ldl
	EXE ?= $(shell uname -m)
	MAKE_EXE_SYMLINK = true
	A = a
	DLL = dylib
	MWINDOWS =
	# workaround for weird prints
	ECHO_NOLF = /bin/echo -n

	# workaround: http://developer.apple.com/qa/qa2007/qa1567.html
	LIBS_GL += -lX11 -dylib_file /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib
	LIBS_GTKGLEXT += -lX11 -dylib_file /System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib
	# workaround: we have no "ldd" for OS X, so...
	LDD =
	OTOOL = otool
else

$(error Unsupported build OS: $(OS))
endif
endif
endif

# VERSION!
RADIANT_VERSION_NUMBER = 1.5.0
RADIANT_VERSION = $(RADIANT_VERSION_NUMBER)n
RADIANT_MAJOR_VERSION = 5
RADIANT_MINOR_VERSION = 0
Q3MAP_VERSION = 2.5.17n

# Executable extension
RADIANT_EXECUTABLE := $(EXE)

GIT_VERSION := $(shell $(GIT) rev-parse --short HEAD $(STDERR_TO_DEVNULL))
ifneq ($(GIT_VERSION),)
	RADIANT_VERSION := $(RADIANT_VERSION)-git-$(GIT_VERSION)
	Q3MAP_VERSION := $(Q3MAP_VERSION)-git-$(GIT_VERSION)
endif

CPPFLAGS += -DRADIANT_VERSION="\"$(RADIANT_VERSION)\"" -DRADIANT_MAJOR_VERSION="\"$(RADIANT_MAJOR_VERSION)\"" -DRADIANT_MINOR_VERSION="\"$(RADIANT_MINOR_VERSION)\"" -DRADIANT_ABOUTMSG="\"$(RADIANT_ABOUTMSG)\"" -DQ3MAP_VERSION="\"$(Q3MAP_VERSION)\"" -DRADIANT_EXECUTABLE="\"$(RADIANT_EXECUTABLE)\""

.PHONY: all
all: \
	dependencies-check \
	binaries \
	install-data \
	install-dll \

.PHONY: dependencies-check
ifeq ($(findstring $(DEPENDENCIES_CHECK),off),off)
dependencies-check:
	@$(ECHO) dependencies checking disabled, good luck...
else
dependencies-check:
	@$(ECHO)
	@if [ x"$(DEPENDENCIES_CHECK)" = x"verbose" ]; then set -x; exec 3>&2; else exec 3$(FD_TO_DEVNULL); fi; \
	failed=0; \
	checkbinary() \
	{ \
		$(ECHO_NOLF) "Checking for $$2 ($$1)... "; \
		$$2 --help >&3 $(STDERR_TO_STDOUT); \
		if [ $$? != 127 ]; then \
			$(ECHO) "found."; \
		else \
			$(ECHO) "not found, please install it or set PATH right!"; \
			$(ECHO) "To see the failed commands, set DEPENDENCIES_CHECK=verbose"; \
			$(ECHO) "To proceed anyway, set DEPENDENCIES_CHECK=off"; \
			failed=1; \
		fi; \
	}; \
	$(ECHO) checking that the build tools exist; \
	checkbinary "bash (or another shell)" "$(SH)"; \
	checkbinary coreutils "$(ECHO)"; \
	checkbinary coreutils "$(ECHO_NOLF)"; \
	checkbinary coreutils "$(CAT)"; \
	checkbinary coreutils "$(MKDIR)"; \
	checkbinary coreutils "$(CP)"; \
	checkbinary coreutils "$(CP_R)"; \
	checkbinary coreutils "$(RM)"; \
	checkbinary coreutils "$(RM_R)"; \
	checkbinary coreutils "$(MV)"; \
	checkbinary coreutils "$(ECHO) test $(TEE_STDERR)"; \
	checkbinary sed "$(SED)"; \
	checkbinary findutils "$(FIND)"; \
	checkbinary diff "$(DIFF)"; \
	checkbinary gcc "$(CC)"; \
	checkbinary g++ "$(CXX)"; \
	checkbinary binutils "$(RANLIB)"; \
	checkbinary binutils "$(AR)"; \
	checkbinary pkg-config "$(PKGCONFIG)"; \
	checkbinary unzip "$(UNZIPPER)"; \
	checkbinary git-core "$(GIT)"; \
	checkbinary subversion "$(SVN)"; \
	checkbinary wget "$(WGET)"; \
	[ "$(OS)" = "Win32" ] && checkbinary mingw32 "$(WINDRES)"; \
	[ -n "$(LDD)" ] && checkbinary libc6 "$(LDD)"; \
	[ -n "$(OTOOL)" ] && checkbinary xcode "$(OTOOL)"; \
	[ "$$failed" = "0" ] && $(ECHO) All required tools have been found!
	@$(ECHO)
	@if [ x"$(DEPENDENCIES_CHECK)" = x"verbose" ]; then set -x; exec 3>&2; else exec 3$(FD_TO_DEVNULL); fi; \
	failed=0; \
	checkheader() \
	{ \
		$(ECHO_NOLF) "Checking for $$2 ($$1)... "; \
		if \
			$(CXX) conftest.cpp $(CFLAGS) $(CXXFLAGS) $(CFLAGS_COMMON) $(CXXFLAGS_COMMON) $(CPPFLAGS) $(CPPFLAGS_COMMON) $$4 -DCONFTEST_HEADER="<$$2>" -DCONFTEST_SYMBOL="$$3" $(TARGET_ARCH) $(LDFLAGS) -c -o conftest.o >&3 $(STDERR_TO_STDOUT); \
		then \
			if \
				$(CXX) conftest.o $(LDFLAGS) $(LDFLAGS_COMMON) $$5 $(LIBS_COMMON) $(LIBS) -o conftest >&3 $(STDERR_TO_STDOUT); \
			then \
				$(RM) conftest conftest.o conftest.d; \
				$(ECHO) "found and links."; \
			else \
				$(RM) conftest.o conftest.d; \
				$(ECHO) "found but does not link, please install it or set PKG_CONFIG_PATH right!"; \
				$(ECHO) "To see the failed commands, set DEPENDENCIES_CHECK=verbose"; \
				$(ECHO) "To proceed anyway, set DEPENDENCIES_CHECK=off"; \
				failed=1; \
			fi; \
		else \
			$(RM) conftest conftest.o conftest.d; \
			$(ECHO) "not found, please install it or set PKG_CONFIG_PATH right!"; \
			$(ECHO) "To see the failed commands, set DEPENDENCIES_CHECK=verbose"; \
			$(ECHO) "To proceed anyway, set DEPENDENCIES_CHECK=off"; \
			failed=1; \
		fi; \
	}; \
	$(ECHO) checking that the dependencies exist; \
	checkheader libjpeg8-dev jpeglib.h jpeg_set_defaults "$(CPPFLAGS_JPEG)" "$(LIBS_JPEG)"; \
	checkheader libglib2.0-dev glib.h g_path_is_absolute "$(CPPFLAGS_GLIB)" "$(LIBS_GLIB)"; \
	checkheader libxml2-dev libxml/xpath.h xmlXPathInit "$(CPPFLAGS_XML)" "$(LIBS_XML)"; \
	checkheader libpng12-dev png.h png_create_read_struct "$(CPPFLAGS_PNG)" "$(LIBS_PNG)"; \
	checkheader "mesa-common-dev (or another OpenGL library)" GL/gl.h glClear "$(CPPFLAGS_GL)" "$(LIBS_GL)"; \
	checkheader libgtk2.0-dev gtk/gtkdialog.h gtk_dialog_run "$(CPPFLAGS_GTK)" "$(LIBS_GTK)"; \
	checkheader libpango1.0-dev pango/pangoft2.h pango_ft2_font_map_new "$(CPPFLAGS_PANGOFT2)" "$(LIBS_PANGOFT2)"; \
	checkheader libgtkglext1-dev gtk/gtkglwidget.h gtk_widget_get_gl_context "$(CPPFLAGS_GTKGLEXT)" "$(LIBS_GTKGLEXT)"; \
	[ "$(OS)" != "Win32" ] && checkheader libc6-dev dlfcn.h dlopen "$(CPPFLAGS_DL)" "$(LIBS_DL)"; \
	checkheader zlib1g-dev zlib.h zlibVersion "$(CPPFLAGS_ZLIB)" "$(LIBS_ZLIB)"; \
	[ "$$failed" = "0" ] && $(ECHO) All required libraries have been found!
	@$(ECHO)
endif

.PHONY: binaries
binaries: \
	binaries-tools \
	binaries-radiant \

.PHONY: binaries-radiant-all
binaries-radiant: \
	binaries-radiant-modules \
	binaries-radiant-plugins \
	binaries-radiant-core \

.PHONY: binaries-radiant-modules
binaries-radiant-modules: \
	$(INSTALLDIR)/modules/archivepak.$(DLL) \
	$(INSTALLDIR)/modules/archivewad.$(DLL) \
	$(INSTALLDIR)/modules/archivezip.$(DLL) \
	$(INSTALLDIR)/modules/entity.$(DLL) \
	$(INSTALLDIR)/modules/image.$(DLL) \
	$(INSTALLDIR)/modules/imagehl.$(DLL) \
	$(INSTALLDIR)/modules/imagepng.$(DLL) \
	$(INSTALLDIR)/modules/imageq2.$(DLL) \
	$(INSTALLDIR)/modules/mapq3.$(DLL) \
	$(INSTALLDIR)/modules/mapxml.$(DLL) \
	$(INSTALLDIR)/modules/md3model.$(DLL) \
	$(INSTALLDIR)/modules/model.$(DLL) \
	$(INSTALLDIR)/modules/shaders.$(DLL) \
	$(INSTALLDIR)/modules/vfspk3.$(DLL) \

.PHONY: binaries-radiant-plugins
binaries-radiant-plugins: \
	$(INSTALLDIR)/plugins/bobtoolz.$(DLL) \
	$(INSTALLDIR)/plugins/brushexport.$(DLL) \
	$(INSTALLDIR)/plugins/prtview.$(DLL) \
	$(INSTALLDIR)/plugins/shaderplug.$(DLL) \
	$(INSTALLDIR)/plugins/sunplug.$(DLL) \
	$(INSTALLDIR)/plugins/ufoaiplug.$(DLL) \

.PHONY: binaries-radiant
binaries-radiant-core: \
	$(INSTALLDIR)/radiant.$(EXE) \
	$(INSTALLDIR)/radiant \

.PHONY: binaries-tools
binaries-tools: \
	binaries-tools-quake2 \
	binaries-tools-quake3 \

.PHONY: binaries-tools-quake2
binaries-tools-quake2: \
	binaries-q2map \
	binaries-qdata3 \
	binaries-h2data \

.PHONY: binaries-q2map
binaries-q2map: \
	$(INSTALLDIR)/q2map.$(EXE) \
	$(INSTALLDIR)/q2map \

.PHONY: binaries-qdata3
binaries-qdata3: \
	$(INSTALLDIR)/qdata3.$(EXE) \
	$(INSTALLDIR)/qdata3 \

.PHONY: binaries-h2data
binaries-h2data: \
	$(INSTALLDIR)/heretic2/h2data.$(EXE) \
	$(INSTALLDIR)/heretic2/h2data \

.PHONY: binaries-tools-quake3
binaries-tools-quake3: \
	binaries-q3data \
	binaries-q3map2 \

.PHONY: binaries-q3data
binaries-q3data: \
	$(INSTALLDIR)/q3data.$(EXE) \
	$(INSTALLDIR)/q3data \

.PHONY: binaries-q3map2
binaries-q3map2: \
	$(INSTALLDIR)/q3map2.$(EXE) \
	$(INSTALLDIR)/q3map2 \


.PHONY: clean
clean:
	$(RM_R) $(INSTALLDIR_BASE)/
	$(FIND) . \( -name \*.o -o -name \*.d -o -name \*.$(DLL) -o -name \*.$(A) -o -name \*.$(EXE) \) -exec $(RM) {} \;
	$(RM) icons/*.rc

%.$(EXE):
	file=$@; $(MKDIR) $${file%/*}
	$(CXX) $^ $(LDFLAGS) $(LDFLAGS_COMMON) $(LDFLAGS_EXTRA) $(LIBS_EXTRA) $(LIBS_COMMON) $(LIBS) -o $@
	[ -z "$(LDD)" ] || [ -z "`$(LDD) -r $@ $(STDERR_TO_STDOUT) $(STDOUT_TO_DEVNULL) $(TEE_STDERR)`" ] || { $(RM) $@; exit 1; }

$(INSTALLDIR)/%: $(INSTALLDIR)/%.$(EXE)
	if $(MAKE_EXE_SYMLINK); then o=$<; $(LN_SNF) $${o##*/} $@; else true; fi

%.$(A):
	$(AR) rc $@ $^
	$(RANLIB) $@

%.$(DLL):
	file=$@; $(MKDIR) $${file%/*}
	$(CXX) $^ $(LDFLAGS) $(LDFLAGS_COMMON) $(LDFLAGS_EXTRA) $(LDFLAGS_DLL) $(LIBS_EXTRA) $(LIBS_COMMON) $(LIBS) -shared -o $@
	[ -z "$(LDD)" ] || [ -z "`$(LDD) -r $@ $(STDERR_TO_STDOUT) $(STDOUT_TO_DEVNULL) $(TEE_STDERR)`" ] || { $(RM) $@; exit 1; }

%.rc: %.ico
	$(ECHO) '1 ICON "$<"' > $@

ifeq ($(OS),Win32)
%.o: %.rc
	$(WINDRES) $< $@
endif

%.o: %.cpp $(if $(findstring $(DEPEND_ON_MAKEFILE),yes),$(wildcard Makefile*),) | dependencies-check
	$(CXX) $< $(CFLAGS) $(CXXFLAGS) $(CFLAGS_COMMON) $(CXXFLAGS_COMMON) $(CPPFLAGS_EXTRA) $(CPPFLAGS_COMMON) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@

%.o: %.c $(if $(findstring $(DEPEND_ON_MAKEFILE),yes),$(wildcard Makefile*),) | dependencies-check
	$(CC) $< $(CFLAGS) $(CFLAGS_COMMON) $(CPPFLAGS_EXTRA) $(CPPFLAGS_COMMON) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@


$(INSTALLDIR)/q3map2.$(EXE): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB) $(LIBS_PNG) $(LIBS_JPEG) $(LIBS_ZLIB)
$(INSTALLDIR)/q3map2.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_PNG) $(CPPFLAGS_JPEG) -Itools/quake3/common -Ilibs -Iinclude
$(INSTALLDIR)/q3map2.$(EXE): \
	tools/quake3/common/cmdlib.o \
	tools/quake3/common/imagelib.o \
	tools/quake3/common/inout.o \
	tools/quake3/common/jpeg.o \
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
	tools/quake3/q3map2/bsp_analyze.o \
	tools/quake3/q3map2/bsp_scale.o \
	tools/quake3/q3map2/bsp_info.o \
	tools/quake3/q3map2/convert_ase.o \
	tools/quake3/q3map2/convert_bsp.o \
	tools/quake3/q3map2/convert_obj.o \
	tools/quake3/q3map2/convert_map.o \
	tools/quake3/q3map2/decals.o \
	tools/quake3/q3map2/exportents.o \
	tools/quake3/q3map2/facebsp.o \
	tools/quake3/q3map2/fixaas.o \
	tools/quake3/q3map2/fog.o \
	tools/quake3/q3map2/help.o \
	tools/quake3/q3map2/image.o \
	tools/quake3/q3map2/leakfile.o \
	tools/quake3/q3map2/light_bounce.o \
	tools/quake3/q3map2/lightmaps_ydnar.o \
	tools/quake3/q3map2/light.o \
	tools/quake3/q3map2/light_trace.o \
	tools/quake3/q3map2/light_ydnar.o \
	tools/quake3/q3map2/main.o \
	tools/quake3/q3map2/map.o \
	tools/quake3/q3map2/minimap.o \
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
	libetclib.$(A) \
	libfilematch.$(A) \
	libl_net.$(A) \
	libmathlib.$(A) \
	libpicomodel.$(A) \
	$(if $(findstring $(OS),Win32),icons/q3map2.o,) \

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
	$(if $(findstring $(OS),Win32),libs/l_net/l_net_wins.o,libs/l_net/l_net_berkley.o) \

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

libetclib.$(A): CPPFLAGS_EXTRA := -Ilibs
libetclib.$(A): \
	libs/etclib.o \

$(INSTALLDIR)/q3data.$(EXE): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB) $(LIBS_ZLIB)
$(INSTALLDIR)/q3data.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_ZLIB) -Itools/quake3/common -Ilibs -Iinclude
$(INSTALLDIR)/q3data.$(EXE): \
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
	libfilematch.$(A) \
	libetclib.$(A) \
	libl_net.$(A) \
	libmathlib.$(A) \
	$(if $(findstring $(OS),Win32),icons/q3data.o,) \

$(INSTALLDIR)/radiant.$(EXE): LDFLAGS_EXTRA := $(MWINDOWS)
$(INSTALLDIR)/radiant.$(EXE): LIBS_EXTRA := $(LIBS_GL) $(LIBS_DL) $(LIBS_XML) $(LIBS_GLIB) $(LIBS_GTK) $(LIBS_GTKGLEXT) $(LIBS_ZLIB) $(LIBS_PANGOFT2)
$(INSTALLDIR)/radiant.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_GL) $(CPPFLAGS_DL) $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_GTKGLEXT) $(CPPFLAGS_PANGOFT2) -Ilibs -Iinclude
$(INSTALLDIR)/radiant.$(EXE): \
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
	$(if $(findstring $(OS),Win32),radiant/multimon.o,) \
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
	$(if $(findstring $(OS),Win32),icons/radiant.o,) \

libfilematch.$(A): CPPFLAGS_EXTRA := -Ilibs
libfilematch.$(A): \
	libs/filematch.o \

libcmdlib.$(A): CPPFLAGS_EXTRA := -Ilibs
libcmdlib.$(A): \
	libs/cmdlib/cmdlib.o \

libprofile.$(A): CPPFLAGS_EXTRA := -Ilibs -Iinclude
libprofile.$(A): \
	libs/profile/file.o \
	libs/profile/profile.o \

libgtkutil.$(A): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_GTKGLEXT) $(CPPFLAGS_PANGOFT2) -Ilibs -Iinclude
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

$(INSTALLDIR)/modules/archivezip.$(DLL): LIBS_EXTRA := $(LIBS_ZLIB)
$(INSTALLDIR)/modules/archivezip.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_ZLIB) -Ilibs -Iinclude
$(INSTALLDIR)/modules/archivezip.$(DLL): \
	plugins/archivezip/archive.o \
	plugins/archivezip/pkzip.o \
	plugins/archivezip/plugin.o \
	plugins/archivezip/zlibstream.o \

$(INSTALLDIR)/modules/archivewad.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/archivewad.$(DLL): \
	plugins/archivewad/archive.o \
	plugins/archivewad/plugin.o \
	plugins/archivewad/wad.o \

$(INSTALLDIR)/modules/archivepak.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/archivepak.$(DLL): \
	plugins/archivepak/archive.o \
	plugins/archivepak/pak.o \
	plugins/archivepak/plugin.o \

$(INSTALLDIR)/modules/entity.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/entity.$(DLL): \
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

$(INSTALLDIR)/modules/image.$(DLL): LIBS_EXTRA := $(LIBS_JPEG)
$(INSTALLDIR)/modules/image.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_JPEG) -Ilibs -Iinclude
$(INSTALLDIR)/modules/image.$(DLL): \
	plugins/image/bmp.o \
	plugins/image/dds.o \
	plugins/image/image.o \
	plugins/image/jpeg.o \
	plugins/image/ktx.o \
	plugins/image/pcx.o \
	plugins/image/tga.o \
	libddslib.$(A) \
	libetclib.$(A) \

$(INSTALLDIR)/modules/imageq2.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/imageq2.$(DLL): \
	plugins/imageq2/imageq2.o \
	plugins/imageq2/wal32.o \
	plugins/imageq2/wal.o \

$(INSTALLDIR)/modules/imagehl.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/imagehl.$(DLL): \
	plugins/imagehl/hlw.o \
	plugins/imagehl/imagehl.o \
	plugins/imagehl/mip.o \
	plugins/imagehl/sprite.o \

$(INSTALLDIR)/modules/imagepng.$(DLL): LIBS_EXTRA := $(LIBS_PNG)
$(INSTALLDIR)/modules/imagepng.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_PNG) -Ilibs -Iinclude
$(INSTALLDIR)/modules/imagepng.$(DLL): \
	plugins/imagepng/plugin.o \

$(INSTALLDIR)/modules/mapq3.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/mapq3.$(DLL): \
	plugins/mapq3/parse.o \
	plugins/mapq3/plugin.o \
	plugins/mapq3/write.o \

$(INSTALLDIR)/modules/mapxml.$(DLL): LIBS_EXTRA := $(LIBS_XML) $(LIBS_GLIB)
$(INSTALLDIR)/modules/mapxml.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) $(CPPFLAGS_GLIB) -Ilibs -Iinclude
$(INSTALLDIR)/modules/mapxml.$(DLL): \
	plugins/mapxml/plugin.o \
	plugins/mapxml/xmlparse.o \
	plugins/mapxml/xmlwrite.o \

$(INSTALLDIR)/modules/md3model.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/md3model.$(DLL): \
	plugins/md3model/md2.o \
	plugins/md3model/md3.o \
	plugins/md3model/md5.o \
	plugins/md3model/mdc.o \
	plugins/md3model/mdlimage.o \
	plugins/md3model/mdl.o \
	plugins/md3model/plugin.o \

$(INSTALLDIR)/modules/model.$(DLL): CPPFLAGS_EXTRA := -Ilibs -Iinclude
$(INSTALLDIR)/modules/model.$(DLL): \
	plugins/model/model.o \
	plugins/model/plugin.o \
	libpicomodel.$(A) \

$(INSTALLDIR)/modules/shaders.$(DLL): LIBS_EXTRA := $(LIBS_GLIB)
$(INSTALLDIR)/modules/shaders.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) -Ilibs -Iinclude
$(INSTALLDIR)/modules/shaders.$(DLL): \
	plugins/shaders/plugin.o \
	plugins/shaders/shaders.o \

$(INSTALLDIR)/modules/vfspk3.$(DLL): LIBS_EXTRA := $(LIBS_GLIB)
$(INSTALLDIR)/modules/vfspk3.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) -Ilibs -Iinclude
$(INSTALLDIR)/modules/vfspk3.$(DLL): \
	plugins/vfspk3/archive.o \
	plugins/vfspk3/vfs.o \
	plugins/vfspk3/vfspk3.o \
	libfilematch.$(A) \

$(INSTALLDIR)/plugins/bobtoolz.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/bobtoolz.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/bobtoolz.$(DLL): \
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

$(INSTALLDIR)/plugins/brushexport.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/brushexport.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/brushexport.$(DLL): \
	contrib/brushexport/callbacks.o \
	contrib/brushexport/export.o \
	contrib/brushexport/interface.o \
	contrib/brushexport/plugin.o \
	contrib/brushexport/support.o \

$(INSTALLDIR)/plugins/prtview.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/prtview.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/prtview.$(DLL): \
	contrib/prtview/AboutDialog.o \
	contrib/prtview/ConfigDialog.o \
	contrib/prtview/LoadPortalFileDialog.o \
	contrib/prtview/portals.o \
	contrib/prtview/prtview.o \
	libprofile.$(A) \

$(INSTALLDIR)/plugins/shaderplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK) $(LIBS_XML)
$(INSTALLDIR)/plugins/shaderplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) $(CPPFLAGS_XML) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/shaderplug.$(DLL): \
	contrib/shaderplug/shaderplug.o \
	libxmllib.$(A) \

$(INSTALLDIR)/plugins/sunplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/sunplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/sunplug.$(DLL): \
	contrib/sunplug/sunplug.o \

$(INSTALLDIR)/qdata3.$(EXE): LIBS_EXTRA := $(LIBS_XML)
$(INSTALLDIR)/qdata3.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/common -Ilibs -Iinclude
$(INSTALLDIR)/qdata3.$(EXE): \
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
	$(if $(findstring $(OS),Win32),icons/qdata3.o,) \

$(INSTALLDIR)/q2map.$(EXE): LIBS_EXTRA := $(LIBS_XML)
$(INSTALLDIR)/q2map.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/common -Ilibs -Iinclude
$(INSTALLDIR)/q2map.$(EXE): \
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
	$(if $(findstring $(OS),Win32),icons/q2map.o,) \

$(INSTALLDIR)/plugins/ufoaiplug.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/ufoaiplug.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/ufoaiplug.$(DLL): \
	contrib/ufoaiplug/ufoai_filters.o \
	contrib/ufoaiplug/ufoai_gtk.o \
	contrib/ufoaiplug/ufoai_level.o \
	contrib/ufoaiplug/ufoai.o \

$(INSTALLDIR)/plugins/bkgrnd2d.$(DLL): LIBS_EXTRA := $(LIBS_GLIB) $(LIBS_GTK)
$(INSTALLDIR)/plugins/bkgrnd2d.$(DLL): CPPFLAGS_EXTRA := $(CPPFLAGS_GLIB) $(CPPFLAGS_GTK) -Ilibs -Iinclude
$(INSTALLDIR)/plugins/bkgrnd2d.$(DLL): \
	contrib/bkgrnd2d/bkgrnd2d.o \
	contrib/bkgrnd2d/dialog.o \
	contrib/bkgrnd2d/plugin.o \

$(INSTALLDIR)/heretic2/h2data.$(EXE): LIBS_EXTRA := $(LIBS_XML)
$(INSTALLDIR)/heretic2/h2data.$(EXE): CPPFLAGS_EXTRA := $(CPPFLAGS_XML) -Itools/quake2/qdata_heretic2/common -Itools/quake2/qdata_heretic2/qcommon -Itools/quake2/qdata_heretic2 -Itools/quake2/common -Ilibs -Iinclude
$(INSTALLDIR)/heretic2/h2data.$(EXE): \
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
	$(if $(findstring $(OS),Win32),icons/h2data.o,) \

.PHONY: install-data
install-data: binaries
	$(MKDIR) $(INSTALLDIR)/games
	$(FIND) $(INSTALLDIR_BASE)/ -name .svn -exec $(RM_R) {} \; -prune
	DOWNLOAD_GAMEPACKS="$(DOWNLOAD_GAMEPACKS)" GIT="$(GIT)" SVN="$(SVN)" WGET="$(WGET)" RM_R="$(RM_R)" MV="$(MV)" UNZIPPER="$(UNZIPPER)" ECHO="$(ECHO)" SH="$(SH)" CP="$(CP)" CP_R="$(CP_R)" $(SH) install-gamepacks.sh "$(INSTALLDIR)"
	$(ECHO) $(RADIANT_MINOR_VERSION) > $(INSTALLDIR)/RADIANT_MINOR
	$(ECHO) $(RADIANT_MAJOR_VERSION) > $(INSTALLDIR)/RADIANT_MAJOR
	$(CP_R) setup/data/tools/* $(INSTALLDIR)/
	$(MKDIR) $(INSTALLDIR)/docs
	$(CP_R) docs/* $(INSTALLDIR)/docs/
	$(FIND) $(INSTALLDIR_BASE)/ -name .svn -exec $(RM_R) {} \; -prune

.PHONY: install-dll
ifeq ($(OS),Win32)
install-dll: binaries
	MKDIR="$(MKDIR)" CP="$(CP)" CAT="$(CAT)" GTKDIR="$(GTKDIR)" WHICHDLL="$(WHICHDLL)" INSTALLDIR="$(INSTALLDIR)" $(SH) $(DLLINSTALL) 
else
install-dll: binaries
	@$(ECHO) No DLL inclusion implemented for this target.
endif

# release building... NOT for general users
# these may use tools not in the list that is checked by the build system
release-src: BUILD_DATE := $(shell date +%Y%m%d)
release-src: INSTALLDIR := netradiant-$(RADIANT_VERSION_NUMBER)-$(BUILD_DATE)
release-src:
	$(GIT) archive --format=tar --prefix=$(INSTALLDIR)/ HEAD | bzip2 > ../$(INSTALLDIR).tar.bz2

release-win32: BUILD_DATE := $(shell date +%Y%m%d)
release-win32: INSTALLDIR := netradiant-$(RADIANT_VERSION_NUMBER)-$(BUILD_DATE)
release-win32:
	$(MAKE) all INSTALLDIR=$(INSTALLDIR) MAKEFILE_CONF=cross-Makefile.conf RADIANT_ABOUTMSG="Official release build" BUILD=release
	7za a -sfx../../../../../../../../../../$(HOME)/7z.sfx ../$(INSTALLDIR)-win32-7z.exe $(INSTALLDIR)/
	chmod 644 ../$(INSTALLDIR)-win32-7z.exe # 7zip is evil
	$(MAKE) clean INSTALLDIR=$(INSTALLDIR) MAKEFILE_CONF=cross-Makefile.conf RADIANT_ABOUTMSG="Official release build" BUILD=release

release-all:
	$(GIT) clean -xdf
	$(MAKE) release-src
	$(MAKE) release-win32

# load dependency files
-include $(shell find . -name \*.d)
