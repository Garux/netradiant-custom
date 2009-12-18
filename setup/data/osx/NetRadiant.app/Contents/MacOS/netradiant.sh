#!/bin/sh

MY_DIRECTORY="${0%/*}" # cut off the script name
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off MacOS
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off Contents

export DYLD_LIBRARY_PATH="$MY_DIRECTORY/Contents/MacOS/install"
export PANGO_RC_FILE="$MY_DIRECTORY/Contents/MacOS/install/pangorc"
export GDK_PIXBUF_MODULE_FILE="$MY_DIRECTORY/Contents/MacOS/install/gdk-pixbuf.loaders"
export FONTCONFIG_FILE="$MY_DIRECTORY/Contents/MacOS/install/fonts.conf"

cd "$MY_DIRECTORY/Contents/MacOS/install"
if [ -x /usr/bin/open-x11 ]; then
	/usr/bin/open-x11 ./radiant.%EXE% "$@" &
else
	./radiant.%EXE% "$@" &
fi
