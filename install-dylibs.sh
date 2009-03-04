#!/bin/sh

set -ex

: ${OTOOL:=otool}
: ${CP:=cp}
: ${INSTALLDIR:=.}

finkgetdeps()
{
	otool -L "$1" | grep /sw/lib | while read -r LIB STUFF; do
		[ -f "$INSTALLDIR/${LIB##*/}" ] && continue
		cp -vL "$LIB" "$INSTALLDIR"
		finkgetdeps "$INSTALLDIR/${LIB##*/}"
	done
}

finkgetdeps "$INSTALLDIR/radiant.ppc"
echo Warning: this only works if only ONE version of gtk-2.0 and pango is installed
cp -vL /sw/lib/gtk-2.0/*/loaders/libpixbufloader-bmp.so "$INSTALLDIR/"
cp -vL /sw/lib/pango/*/modules/pango-basic-fc.so "$INSTALLDIR/"
cp -vL /sw/lib/pango/*/modules/pango-basic-x.so "$INSTALLDIR/"
