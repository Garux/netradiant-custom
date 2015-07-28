#!/bin/sh

: ${ECHO:=echo}
: ${SH:=sh}
: ${CP:=cp}
: ${CP_R:=cp -r}
: ${SOURCE_DIR:=.}

dest=$1

case "$DOWNLOAD_GAMEPACKS" in
	yes)
		LICENSEFILTER=GPL BATCH=1 $SH "$SOURCE_DIR/download-gamepacks.sh"
		;;
	all)
		BATCH=1 $SH "$SOURCE_DIR/download-gamepacks.sh"
		;;
	*)
		;;
esac

set -e
for GAME in games/*; do
	if [ "$GAME" = "games/*" ]; then
		$ECHO "Game packs not found, please run"
		$ECHO "  $SOURCE_DIR/download-gamepacks.sh"
		$ECHO "and then try again!"
	else
		$SH "$SOURCE_DIR/install-gamepack.sh" "$GAME" "$dest"
	fi
done
