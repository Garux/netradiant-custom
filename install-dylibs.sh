#!/bin/sh

set -ex

: ${OTOOL:=otool}
: ${CP:=cp}
: ${INSTALLDIR:=.}

finkgetdeps()
{
	otool -L "$1" | grep /sw/lib | while read -r LIB STUFF; do
		[ -z "${LIB##*:}" ] && continue # first line
		[ -f "$INSTALLDIR/${LIB##*/}" ] && continue
		cp -vL "$LIB" "$INSTALLDIR"
		finkgetdeps "$LIB"
	done
}

finkgetdeps "$INSTALLDIR/radiant.ppc"
echo Warning: this only works if only ONE version of gtk-2.0 and pango is installed

for LIB in /sw/lib/gtk-2.0/*/loaders/libpixbufloader-bmp.so; do
	LAST=$LIB
done
cp -L "$LAST" "$INSTALLDIR"
finkgetdeps "$LAST"

for LIB in /sw/lib/pango/*/modules/pango-basic-fc.so; do
	LAST=$LIB
done
cp -L "$LAST" "$INSTALLDIR"
finkgetdeps "$LAST"

for LIB in /sw/lib/pango/*/modules/pango-basic-x.so; do
	LAST=$LIB
done
cp -L "$LAST" "$INSTALLDIR"
finkgetdeps "$LAST"
