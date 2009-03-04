#!/bin/sh

MY_DIRECTORY="${0%/*}" # cut off the script name
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off MacOS
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off Contents

export DYLD_LIBRARY_PATH="$MY_DIRECTORY/Contents/MacOS/install"
export PANGO_RC_FILE="$MY_DIRECTORY/Contents/MacOS/pangorc"
export GDK_PIXBUF_MODULE_FILE="$MY_DIRECTORY/Contents/MacOS/gdk-pixbuf.loaders"

cd "$MY_DIRECTORY/Contents/MacOS/install"

# autodetect nexuiz installs
NEX_DIRECTORY="/${MY_DIRECTORY%/*}"
while :; do
	if [ -z "$NEX_DIRECTORY" ]; then
		break
	fi
	if [ -f "$NEX_DIRECTORY/data/common-spog.pk3" ]; then
		if [ -d "$NEX_DIRECTORY/Nexuiz.app" ]; then
			break
		fi
	fi
	NEX_DIRECTORY=${NEX_DIRECTORY%/*}
done
case "$NEX_DIRECTORY" in
	//*)
		NEX_DIRECTORY=${NEX_DIRECTORY#/}
		set -- -global-gamefile nexuiz.game -nexuiz.game-EnginePath "$NEX_DIRECTORY/"
		# -global-gamePrompt false?
		;;
	*)
		set --
		;;
esac

if [ -x /usr/bin/open-x11 ]; then
	exec /usr/bin/open-x11 ./radiant.ppc "$@"
else
	exec ./radiant.ppc "$@"
fi
