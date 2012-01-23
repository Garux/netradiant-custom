#!/bin/sh

set -ex

: ${OTOOL:=otool}
: ${CP:=cp}
: ${INSTALLDIR:=.}
: ${EXE:=ppc}
: ${MACLIBDIR:=/sw/lib}
: ${CAT:=cat}


finkgetdeps()
{
	otool -L "$1" | grep "$MACLIBDIR" | while read -r LIB STUFF; do
		[ -z "${LIB##*:}" ] && continue # first line
		[ -f "$INSTALLDIR/${LIB##*/}" ] && continue
		cp -vL "$LIB" "$INSTALLDIR"
		finkgetdeps "$LIB"
	done
}


finkgetdeps "$INSTALLDIR/radiant.$EXE"
echo Warning: this only works if only ONE version of gtk-2.0 and pango is installed

getlib()
{
	LAST=
	for LIB in "$@"; do
		[ -f "$LIB" ] || continue
		LAST=$LIB
	done
	cp -L "$LAST" "$INSTALLDIR"
	finkgetdeps "$LAST"
}

getlib "$MACLIBDIR"/gtk-2.0/*/loaders/libpixbufloader-bmp.so "$MACLIBDIR"/gdk-pixbuf-2.0/*/loaders/libpixbufloader-bmp.so
getlib "$MACLIBDIR"/pango/*/modules/pango-basic-fc.so
getlib "$MACLIBDIR"/pango/*/modules/pango-basic-x.so

#cp -L "$MACLIBDIR"/../etc/fonts/fonts.conf "$INSTALLDIR"
#cp -L "$MACLIBDIR"/../etc/fonts/fonts.dtd "$INSTALLDIR"
#cp -L "$MACLIBDIR"/../etc/gtk-2.0/gdk-pixbuf.loaders "$INSTALLDIR"
#cp -L "$MACLIBDIR"/../etc/pango/pangorc "$INSTALLDIR"

$CAT > "$INSTALLDIR/../netradiant.sh" <<EOF
#!/bin/sh

MY_DIRECTORY="\${0%/*}" # cut off the script name
MY_DIRECTORY="\${MY_DIRECTORY%/*}" # cut off MacOS
MY_DIRECTORY="\${MY_DIRECTORY%/*}" # cut off Contents

export DYLD_LIBRARY_PATH="\$MY_DIRECTORY/Contents/MacOS/install"
export PANGO_RC_FILE="\$MY_DIRECTORY/Contents/MacOS/install/pangorc"
export GDK_PIXBUF_MODULE_FILE="\$MY_DIRECTORY/Contents/MacOS/install/gdk-pixbuf.loaders"
export FONTCONFIG_FILE="\$MY_DIRECTORY/Contents/MacOS/install/fonts.conf"

cd "\$MY_DIRECTORY/Contents/MacOS/install"
if [ -x /usr/bin/open-x11 ]; then
	env LC_ALL="en_US.UTF-8" /usr/bin/open-x11 ./radiant.$EXE "$@" &
else
	env LC_ALL="en_US.UTF-8" ./radiant.$EXE "$@" &
fi
EOF

chmod 755 "$INSTALLDIR/../netradiant.sh"

