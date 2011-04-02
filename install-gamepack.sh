#!/bin/sh

# installs a game pack
# Usage:
#   install-gamepack.sh gamepack installdir

set -ex

: ${CP:=cp}
: ${CP_R:=cp -r}

pack=$1
dest=$2

if [ -d "$pack/tools" ]; then
	pack="$pack/tools"
fi
for GAMEFILE in "$pack/games"/*.game; do
	if [ x"$GAMEFILE" != x"$pack/games/*.game" ]; then
		$CP "$GAMEFILE" "$dest/games/"
	fi
done
for GAMEDIR in "$pack"/*.game; do
	if [ x"$GAMEDIR" != x"$pack/*.game" ]; then
		$CP_R "$GAMEDIR" "$dest/"
	fi
done
