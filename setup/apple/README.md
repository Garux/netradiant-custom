NetRadiant for Apple OS X
========================

This directory provides packaging steps for NetRadiant for OS X. This document describes compiling the application on OSX as well as generating distributable bundles using the framework provided in this directory.

Dependencies & Compilation
--------------------------

Directions for OS X Yosemite 10.10 - your mileage may vary:

- Install [MacPorts](http://macports.org).
- Install [XQuartz](http://xquartz.macosforge.org/)

- Install dependencies with MacPorts:

```
sudo port install dylibbundler pkgconfig gtkglext
```

- Get the NetRadiant code and compile:

```
git clone https://gitlab.com/xonotic/netradiant.git
cd netradiant/
make
```

- Run the build:

(from the netradiant/ directory)
```
./install/radiant
```

XQuartz note: on my configuration XQuartz doesn't automatically start for some reason. I have to open another terminal, and run the following command: `/Applications/Utilities/XQuartz.app/Contents/MacOS/X11.bin`, then start radiant. 
    
Building NetRadiant.app
-----------------------

The `Makefile` in the 'setup/apple/' directory will produce a distributable .app bundle for NetRadiant using `dylibbundler`:

```
make
make image
```

Getting help
------------

IRC: Quakenet #xonotic, or post something on the issue tracker..
