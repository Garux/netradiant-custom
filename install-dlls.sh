#!/bin/sh

set -ex

: ${WHICHDLL:=which}
: ${GTKDIR:=/gtk}
: ${CP:=cp}
: ${CAT:=cat}
: ${MKDIR:=mkdir -p}
: ${INSTALLDIR:=.}

for DLL in \
	intl.dll \
	libatk-1.0-0.dll \
	libcairo-2.dll \
	libgdk-win32-2.0-0.dll \
	libgdk_pixbuf-2.0-0.dll \
	libgdkglext-win32-1.0-0.dll \
	libglib-2.0-0.dll \
	libgmodule-2.0-0.dll \
	libgobject-2.0-0.dll \
	libgtk-win32-2.0-0.dll \
	libgtkglext-win32-1.0-0.dll \
	libpango-1.0-0.dll \
	libpangocairo-1.0-0.dll \
	libpangowin32-1.0-0.dll \
	libpng12-0.dll \
	libxml2-2.dll \
	zlib1.dll \
; do
	$CP "`$WHICHDLL $DLL`" $INSTALLDIR/
done

$CP "$GTKDIR/lib/gtk-2.0/2.10.0/loaders/libpixbufloader-bmp.dll" $INSTALLDIR/libgdk-win32-2.0-0-pixbufloader-bmp.dll
$MKDIR $INSTALLDIR/etc/gtk-2.0
$CAT > $INSTALLDIR/etc/gtk-2.0/gdk-pixbuf.loaders <<'EOF'
# GdkPixbuf Image Loader Modules file
#
#

"libgdk-win32-2.0-0-pixbufloader-bmp.dll"
"bmp" 5 "gtk20" "The BMP image format"
"image/bmp" "image/x-bmp" "image/x-MS-bmp" ""
"bmp" ""
"BM" "" 100

EOF
