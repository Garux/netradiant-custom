#!/bin/sh

# Usage:
#   sh download-gamepack.sh
#   LICENSEFILTER=GPL BATCH=1 sh download-gamepack.sh

pack()
{
	pack=$1; shift
	license=$1; shift
	sourcetype=$1; shift
	source=$1; shift

	if [ -d "games/$pack" ]; then
		echo "Updating $pack..."
		case "$sourcetype" in
			svn)
				svn update "games/$pack" "$@"
				;;
		esac
		return
	fi

	echo
	echo "Available pack: $pack"
	echo "  License: $license"
	echo "  Download via $sourcetype from $source"
	echo
	case " $PACKFILTER " in
		"  ")
			;;
		*" $pack "*)
			;;
		*)
			echo "Pack $pack rejected because it is not in PACKFILTER."
			return
			;;
	esac
	case " $LICENSEFILTER " in
		"  ")
			;;
		*" $license "*)
			;;
		*)
			echo "Pack $pack rejected because its license is not in LICENSEFILTER."
			return
			;;
	esac
	case "$BATCH" in
		'')
			while :; do
				echo "Download this pack? (y/n)"
				read -r P
				case "$P" in
					y*)
						break
						;;
					n*)
						return
						;;
				esac
			done
			;;
		*)
			;;
	esac
	
	echo "Downloading $pack..."
	case "$sourcetype" in
		svn)
			svn checkout "$source" "games/$pack" "$@"
			;;
	esac
}

mkdir -p games
pack NexuizPack      GPL         svn svn://svn.icculus.org/nexuiz/trunk/misc/netradiant-NexuizPack
pack Quake2WorldPack GPL         svn svn://jdolan.dyndns.org/quake2world/trunk/gtkradiant
pack DarkPlacesPack  GPL         svn https://zerowing.idsoftware.com/svn/radiant.gamepacks/DarkPlacesPack/branches/1.5/
pack WarsowPack      GPL         svn http://opensvn.csie.org/warsowgamepack/netradiant/games/WarsowPack/
pack Q3Pack          proprietary svn https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q3Pack/trunk/ -r29
pack UFOAIPack       proprietary svn https://zerowing.idsoftware.com/svn/radiant.gamepacks/UFOAIPack/branches/1.5/
