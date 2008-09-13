if [ -n "$CROSS" ]; then
	CFLAGS="-I$HOME/mingw/include"
	LDFLAGS="-L$HOME/mingw/lib -lws2_32 -lole32 -lintl -liconv"
	XML2_CFLAGS="-I$HOME/mingw/include/libxml2"
	GLIB2_CFLAGS="-I$HOME/mingw/include/glib-2.0 -I$HOME/mingw/lib/glib-2.0/include"
	CC=i586-mingw32msvc-gcc
	CXX=i586-mingw32msvc-g++
	netlib=libs/l_net/l_net_wins.c
	SUFFIX=.exe
else
	CFLAGS=""
	LDFLAGS="-lpthread"
	XML2_CFLAGS="-I/usr/include/libxml2"
	GLIB2_CFLAGS="-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include"
	CC=gcc
	CXX=g++
	netlib=libs/l_net/l_net_berkley.c
	SUFFIX=
fi

LIBS="-lpng -lmhash -lxml2 -lglib-2.0"
CFLAGS_COMMON="-O3 -ffast-math -fno-unsafe-math-optimizations -fno-strict-aliasing -DQ_NO_STLPORT" # -fvisibility=hidden
CFLAGS_LIBS="$CFLAGS $CFLAGS_COMMON $XML2_CFLAGS $GLIB2_CFLAGS -Iinclude -Ilibs -Ilibs/jpeg6"
CFLAGS_Q3MAP2="-Itools/quake3/common $CFLAGS_LIBS"

temp="obj/"
mkdir -p $temp
OBJECTS=
compile()
{
	eval sourcefile=\$\{$#\}
	objectfile=${sourcefile%.*}
	objectfile=`echo "$objectfile" | tr / -`
	objectfile="$temp/$objectfile.o"
	OBJECTS="$OBJECTS $objectfile"
	if ! [ -f "$objectfile" ]; then
		echo "$1	$sourcefile"
		"$@" -c -o $objectfile || exit 1
	fi
}
link()
{
	out=$1
	shift
	linker=$1
	shift
	echo "$linker	$out"

	"$linker" $OBJECTS "$@" -o $out$SUFFIX || exit 1
}

compile $CXX $CFLAGS_LIBS libs/cmdlib/cmdlib.cpp
compile $CC $CFLAGS_LIBS libs/mathlib/bbox.c
compile $CC $CFLAGS_LIBS libs/mathlib/m4x4.c
compile $CC $CFLAGS_LIBS libs/mathlib/mathlib.c
compile $CC $CFLAGS_LIBS libs/mathlib/ray.c
compile $CC $CFLAGS_LIBS libs/l_net/l_net.c
compile $CC $CFLAGS_LIBS $netlib
compile $CC $CFLAGS_LIBS libs/ddslib/ddslib.c
compile $CC $CFLAGS_LIBS libs/picomodel/picointernal.c
compile $CC $CFLAGS_LIBS libs/picomodel/picomodel.c
compile $CC $CFLAGS_LIBS libs/picomodel/picomodules.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_3ds.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_ase.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_fm.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_lwo.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_md2.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_md3.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_mdc.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_ms3d.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_obj.c
compile $CC $CFLAGS_LIBS libs/picomodel/pm_terrain.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/clip.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/envelope.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/list.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/lwio.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/lwo2.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/lwob.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/pntspols.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/surface.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/vecmath.c
compile $CC $CFLAGS_LIBS libs/picomodel/lwo/vmap.c
compile $CC $CFLAGS_LIBS libs/md5lib/md5lib.c
compile $CXX $CFLAGS_LIBS libs/jpeg6/jcomapi.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdapimin.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdapistd.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdatasrc.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdcoefct.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdcolor.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jddctmgr.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdhuff.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdinput.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdmainct.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdmarker.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdmaster.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdpostct.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdsample.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jdtrans.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jerror.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jfdctflt.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jidctflt.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jmemmgr.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jmemnobs.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jpgload.cpp
compile $CXX $CFLAGS_LIBS libs/jpeg6/jutils.cpp
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/bspfile_abstract.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/bspfile_ibsp.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/bspfile_rbsp.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/image.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/main.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/mesh.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/model.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/path_init.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/shaders.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/surface_extra.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/cmdlib.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/imagelib.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/inout.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/mutex.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/polylib.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/scriplib.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/threads.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/unzip.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/common/vfs.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/brush.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/brush_primit.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/bsp.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/decals.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/facebsp.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/fog.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/leakfile.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/map.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/patch.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/portals.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/prtfile.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/surface.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/surface_foliage.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/surface_fur.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/surface_meta.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/tjunction.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/tree.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/writebsp.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/light.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/light_bounce.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/light_trace.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/light_ydnar.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/lightmaps_ydnar.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/vis.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/visflow.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/convert_ase.c
compile $CC $CFLAGS_Q3MAP2 tools/quake3/q3map2/convert_map.c
link q3map2 $CXX $LIBS $LDFLAGS
