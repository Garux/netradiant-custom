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
	$CP "$GAMEFILE" "$dest/games/"
done
for GAMEDIR in "$pack"/*.game; do
	$CP_R "$GAMEDIR" "$dest/"
done
