#!/bin/sh

# Usage:
#   sh download-gamepack.sh
#   LICENSEFILTER=GPL BATCH=1 sh download-gamepack.sh

: ${GIT:=git}
: ${SVN:=svn}
: ${WGET:=wget}
: ${ECHO:=echo}
: ${MKDIR:=mkdir}
: ${RM_R:=rm -f -r}
: ${MV:=mv}
: ${UNZIPPER:=unzip}

set -e

extra_urls()
{
	if [ -f "$1/extra-urls.txt" ]; then
		while IFS="	" read -r FILE URL; do
			$WGET -O "$1/$FILE" "$URL"
		done < "$1/extra-urls.txt"
	fi
}

pack()
{
	pack=$1; shift
	license=$1; shift
	sourcetype=$1; shift
	source=$1; shift

	if [ -d "games/$pack" ]; then
		$ECHO "Updating $pack..."
		case "$sourcetype" in
			svn)
				$SVN update "games/$pack" "$@" || true
				;;
			zip1)
				$RM_R zipdownload
				$MKDIR zipdownload
				cd zipdownload
				$WGET "$source" "$@" || true
				$UNZIPPER *.zip || true
				cd ..
				$RM_R "games/$pack"
				$MKDIR "games/$pack"
				$MV zipdownload/*/* "games/$pack/" || true
				$RM_R zipdownload
				;;
			gitdir)
				$RM_R "games/$pack"
				cd games
				$GIT archive --remote="$source" --prefix="$pack/" "$2":"$1" | tar xvf - || true
				cd ..
				;;
			git)
				cd "games/$pack"
				$GIT pull || true
				cd ../..
				;;
		esac
		extra_urls "games/$pack"
		return
	fi

	$ECHO
	$ECHO "Available pack: $pack"
	$ECHO "  License: $license"
	$ECHO "  Download via $sourcetype from $source"
	$ECHO
	case " $PACKFILTER " in
		"  ")
			;;
		*" $pack "*)
			;;
		*)
			$ECHO "Pack $pack rejected because it is not in PACKFILTER."
			return
			;;
	esac
	case " $LICENSEFILTER " in
		"  ")
			if [ "$license" == "allinone" ]; then
				$ECHO "All-in-one pack skipped: only downloading it alone without other packs."
				$ECHO
				return
			fi
			;;
		*" $license "*)
			;;
		*)
			$ECHO "Pack $pack rejected because its license is not in LICENSEFILTER."
			return
			;;
	esac
	case "$BATCH" in
		'')
			while :; do
				$ECHO "Download this pack? (y/n)"
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

	$ECHO "Downloading $pack..."
	case "$sourcetype" in
		svn)
			$SVN checkout "$source" "games/$pack" "$@" || true
			;;
		zip1)
			$RM_R zipdownload
			$MKDIR zipdownload
			cd zipdownload
			$WGET "$source" "$@" || true
			$UNZIPPER *.zip || true
			cd ..
			$MKDIR "games/$pack"
			$MV zipdownload/*/* "games/$pack/" || true
			$RM_R zipdownload
			;;
		gitdir)
			cd games
			$GIT archive --remote="$source" --prefix="$pack/" "$2":"$1" | tar xvf - || true
			cd ..
			;;
		git)
			cd games
			$GIT clone "$source" "$pack" || true
			cd ..
			;;
	esac
	extra_urls "games/$pack"
	good=false
	for D in "games/$pack"/*.game; do
		if [ -d "$D" ]; then
			good=true
		fi
	done
	$good || rm -rf "$D"
}

mkdir -p games
pack AlienArenaPack    GPL         svn    https://svn.code.sf.net/p/alienarena-cc/code/trunk/tools/netradiant_gamepack/AlienArenaPack
pack DarkPlacesPack    GPL         svn    svn://svn.icculus.org/gtkradiant-gamepacks/DarkPlacesPack/branches/1.5/
pack Doom3Pack         proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/Doom3Pack/branches/1.5/
pack ETPack            proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/ETPack/branches/1.5/
pack Heretic2Pack      proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/Her2Pack/branches/1.5/
pack JediAcademyPack   proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/JAPack/branches/1.5/ #600Mb dl
#pack JediOutcastPack  proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/JK2Pack/branches/1.4/ #not 1.5 pack
#pack KingpinPack      unknown     zip1   http://download.kingpin.info/kingpin/editing/maps/map_editors/NetRadiant/addon/Kingpinpack.zip #dl error: wrong certificate
pack KingpinPack       unknown     git    https://gitlab.com/netradiant/gamepacks/kingpin-mapeditor-support.git
pack NeverballPack     proprietary zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/NeverballPack.zip
#pack NeverballPack    proprietary git    https://gitlab.com/netradiant/gamepacks/neverball-mapeditor-support.git
pack NexuizPack        GPL         gitdir git://git.icculus.org/divverent/nexuiz.git misc/netradiant-NexuizPack master
#pack OpenArenaPack    unknown     zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/OpenArenaPack.zip
pack OpenArenaPack     GPL         git    https://github.com/NeonKnightOA/oagamepack.git
pack OsirionPack       GPL         zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/OsirionPack.zip
pack PreyPack          proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/PreyPack/trunk/
pack Q3RallyPack       proprietary svn    https://svn.code.sf.net/p/q3rallysa/code/tools/radiant-config/radiant15-netradiant/
pack Quake2Pack        proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/Q2Pack/branches/1.5/
pack Quake3Pack        proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/Q3Pack/trunk/ -r29
#pack Quake3Pack       proprietary git    https://gitlab.com/netradiant/gamepacks/quake3-mapeditor-support.git
pack Quake4Pack        proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/Q4Pack/branches/1.5/
pack QuakeLivePack     proprietary git    https://gitlab.com/netradiant/gamepacks/quakelive-mapeditor-support.git
#pack QuakePack        proprietary zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/QuakePack.zip
pack QuakePack         GPL         zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/Quake1Pack.zip
#pack Quake2WorldPack  GPL         svn    svn://svn.icculus.org/gtkradiant-gamepacks/Q2WPack/branches/1.5/ #renamed as Quetoo
#pack Quake2WorldPack  GPL         svn    svn://jdolan.dyndns.org/quake2world/trunk/gtkradiant #renamed as Quetoo
pack QuetooPack        GPL         svn    svn://svn.icculus.org/gtkradiant-gamepacks/QuetooPack/branches/1.5/
pack SmokinGunsPack    unknown     git    https://github.com/smokin-guns/smokinguns-mapeditor-support.git
pack SoF2Pack          unknown     git    https://gitlab.com/netradiant/gamepacks/sof2-mapeditor-support.git
pack STVEFPack         unknown     git    https://gitlab.com/netradiant/gamepacks/stvef-mapeditor-support.git
#pack TremulousPack    proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/TremulousPack/branches/1.5/
pack TremulousPack     proprietary zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/TremulousPack.zip
#pack TremulousPack    proprietary git    https://gitlab.com/netradiant/gamepacks/tremulous-mapeditor-support.git
pack TurtleArenaPack   proprietary git    https://github.com/Turtle-Arena/turtle-arena-radiant-pack.git
pack UFOAIPack         proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/UFOAIPack/branches/1.5/
#pack UnvanquishedPack unknown     zip1   http://ingar.intranifty.net/gtkradiant/files/gamepacks/UnvanquishedPack.zip
#pack UnvanquishedPack BSD         svn    https://github.com/Unvanquished/unvanquished-mapeditor-support.git/trunk/build/netradiant
pack UnvanquishedPack  BSD         git    https://github.com/Unvanquished/unvanquished-mapeditor-support.git
pack UrbanTerrorPack   unknown     git    https://gitlab.com/netradiant/gamepacks/urbanterror-mapeditor-support.git
#pack WarforkPack      GPL         zip1   https://cdn.discordapp.com/attachments/611741789237411850/659512520553267201/netradiant_warfork_gamepack.zip
pack WarforkPack       GPL         git    https://gitlab.com/netradiant/gamepacks/warfork-mapeditor-support.git
#pack WarsowPack       GPL         svn    https://svn.bountysource.com/wswpack/trunk/netradiant/games/WarsowPack/
#pack WarsowPack       GPL         zip1   http://ingar.intranifty.net/files/netradiant/gamepacks/WarsowPack.zip
pack WarsowPack        GPL         git    https://github.com/Warsow/NetRadiantPack.git
pack WolfPack          proprietary svn    svn://svn.icculus.org/gtkradiant-gamepacks/WolfPack/branches/1.5/
pack WoPPack           proprietary git    https://github.com/PadWorld-Entertainment/wop-mapeditor-support.git
pack XonoticPack       GPL         git    https://gitlab.com/xonotic/netradiant-xonoticpack.git
pack ZEQ2LitePack      unknown     git    https://gitlab.com/netradiant/gamepacks/zeq2lite-mapeditor-support.git

pack NRCPack           allinone    zip1    https://www.dropbox.com/s/b1xpajzfa6yjlzf/netradiant-custom-extra-gamepacks.zip
