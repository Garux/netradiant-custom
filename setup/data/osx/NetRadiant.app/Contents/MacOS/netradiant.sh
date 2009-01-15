#!/bin/sh

MY_DIRECTORY="${0%/*}" # cut off the script name
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off MacOS
MY_DIRECTORY="${MY_DIRECTORY%/*}" # cut off Contents

#export DYLD_LIBRARY_PATH="$MY_DIRECTORY/Contents/MacOS"

cd "$MY_DIRECTORY/Contents/MacOS/NetRadiant/install"
exec /usr/bin/open-x11 ./radiant.ppc
