#!/bin/sh

# Usage:
#   sh download-gamepack.sh
#   LICENSEFILTER=GPL BATCH=1 sh download-gamepack.sh

set -e

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
			zip1)
				rm -rf zipdownload
				mkdir zipdownload
				cd zipdownload
				wget "$source" "$@"
				unzip *
				cd ..
				rm -rf "games/$pack"
				mkdir "games/$pack"
				mv zipdownload/*/* "games/$pack/"
				rm -rf zipdownload
				;;
			gitdir)
				rm -rf "games/$pack"
				cd games
				git archive --remote="$source" --prefix="$pack/" "$2":"$1" | tar xvf -
				cd ..
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
		zip1)
			rm -rf zipdownload
			mkdir zipdownload
			cd zipdownload
			wget "$source" "$@"
			unzip *
			cd ..
			mkdir "games/$pack"
			mv zipdownload/*/* "games/$pack/"
			rm -rf zipdownload
			;;
		gitdir)
			rm -rf "games/$pack"
			git archive --remote="$source" --prefix="$pack/" "$2":"$1" | tar xvf -
			;;
	esac
}

mkdir -p games
pack DarkPlacesPack  GPL         svn    https://zerowing.idsoftware.com/svn/radiant.gamepacks/DarkPlacesPack/branches/1.5/
pack NexuizPack      GPL         gitdir git://git.icculus.org/divverent/nexuiz.git misc/netradiant-NexuizPack master
pack OpenArenaPack   unknown     zip1   http://ingar.satgnu.net/files/gtkradiant/gamepacks/OpenArenaPack.zip
pack Q3Pack          proprietary svn    https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q3Pack/trunk/ -r29
pack Quake2Pack      proprietary zip1   http://ingar.satgnu.net/files/gtkradiant/gamepacks/Quake2Pack.zip
pack Quake2WorldPack GPL         svn    svn://jdolan.dyndns.org/quake2world/trunk/gtkradiant
pack QuakePack       proprietary zip1   http://ingar.satgnu.net/files/gtkradiant/gamepacks/QuakePack.zip
pack TremulousPack   proprietary zip1   http://ingar.satgnu.net/files/gtkradiant/gamepacks/TremulousPack.zip
pack UFOAIPack       proprietary svn    https://zerowing.idsoftware.com/svn/radiant.gamepacks/UFOAIPack/branches/1.5/
pack WarsowPack      GPL         svn    http://opensvn.csie.org/warsowgamepack/netradiant/games/WarsowPack/
