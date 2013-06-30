	
NETRADIANT

	NetRadiant is a fork of the well-known map editor for Q3 based games,
	GtkRadiant 1.5. The focus is put on stabilizing and bugfixing the 
	included map compiler, q3map2, so it can become a reliable tool for map authors.
	
	I did not write this program, it is the result of the hard work of many people:
	ID software, the GtkRadiant and NetRadiant developers and anyone who might
	have contributed.

	This packages contains the NetRadiant application and build tools executables,
	the GtkRadiant editor documentation, and several additional gamepacks.
	I provide these packages to anyone who might be interested in making maps, 
	to save them trouble of compiling radiant and finding the necessary game files.
	
	Note: 
	Depending on the game, you might need additional game data for map editing.
	
WEBSITES

	Ingar's NetRadiant packages
	http://ingar.satgnu.net/gtkradiant/
	
	NetRadiant homepage
	http://www.icculus.org/netradiant

SOURCE

	git://git.xonotic.org/xonotic/netradiant.git
	
	http://ingar.satgnu.net/gtkradiant/files/gamepacks

CREDITS

	ID software for the GtkRadiant GPL release.
	
	The GtkRadiant developers and contributors

	Rudolf "divVerent" Polzer for NetRadiant.

	Martin "Mattn" Gerhardy for talk, advice and patches

GAMEPACKS

	DarkPlaces gamepack for GtkRadiant 1.5 (GPL)
	https://zerowing.idsoftware.com/svn/radiant.gamepacks/DarkPlacesPack/branches/1.5/
	
	Quake gamepack for GtkRadiant 1.5
	https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q1Pack/trunk/

	Quake 2 gamepack for GtkRadiant 1.5
	https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q2Pack/branches/1.5/

	Quake 3 gamepack for GtkRadiant 1.5
	(unknown)
	Quake 3 support files can be downloaded from
	https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q3Pack/trunk/install/

	Quake2World gamepack for GtkRadiant 1.5
	http://quake2world.net
	(included with source)

	Nexuiz gamepack for NetRadiant
	git://git.icculus.org/divverent/nexuiz.git misc/netradiant-NexuizPack

	Open Arena gamepack for NetRadiant
	by sago007
	http://openarena.ws/board/index.php?topic=2722.0

	Project::OSiRiON
	http://osirion.org/
	(included with game data)

	Tremulous gamepack for GtkRadiant 1.5
	http://tremmapping.pbwiki.com/
	(this package includes an updated version)
	Tremulous support files can be downloaded from
	http://ingar.satgnu.net/gtkradiant

	UFO: Alien Invasion gamepack for GtkRadiant 1.5
	http://ufoai.sourceforge.net/
	(included with source)

	Warsow gamepack for NetRadiant
	https://svn.bountysource.com/wswpack/trunk/netradiant/games/WarsowPack/

	Xonotic gamepack for NetRadiant
	git://git.xonotic.org/xonotic/netradiant-xonoticpack.git
	
MANUAL

	GtkRadiant Editor Manual
	https://zerowing.idsoftware.com/svn/radiant.gamepacks/Q3Rad_Manual/trunk

CHANGES

	2013-06-30 
		updated to the latest NetRadiant git revision
		
		features added in the ingar/netradiant-ing branch:
		- do not disable Aero on windows
		- support for WebP images in NetRadiant and q3map2
		- replaced BMP icons with PNG icons
		- have the patch texture scale dialog remember last used values

	2012-07-05
		updated to the latest NetRadiant git revision
		added support for Gtk theme engines on win32
		
	2011-03-11
		updated to the latest NetRadiant git revision
		corrected a segfault in q3map2 while compiling with certain options
		added missing libstdc++ dll to the win32 package
		added Ubuntu linux build

	2011-02-20
		updated to the latest NetRadiant git revision
		added MacOS build

	2010-02-14
		updated to latest NetRadiant git revision
		linked with libpng14

	2009-08-13, revision 395
		updated to NetRadiant svn revision 395
		corrected a problem with compiler optimization flags on linux

	2009-07-26, revision 392
		updated to NetRadiant SVN revision 392
		updated Tremulous build menu items
		corrected a problem with tools executable extensions
		added README

LICENSE

	NetRadiant is licensed under the General Public License, 
	version 2.0, as described in the included GPL file.

	The license of the gamepacks isn't always clear, although all
	files can be freely downloaded from the internet.
