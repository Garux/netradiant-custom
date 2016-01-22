NetRadiant
==========

The open source, cross platform level editor for idtech games (NetRadiant fork)

# Getting the Sources

The latest source is available from the git repository:
https://gitlab.com/xonotic/netradiant.git

The git client can be obtained from the Git website:
http://git-scm.org

To get a copy of the source using the commandline git client:
```
git clone https://gitlab.com/xonotic/netradiant.git
cd netradiant
```

See also https://gitlab.com/xonotic/netradiant/ for a source browser, issues and more.

# Dependencies

 * OpenGL
 * LibXml2
 * GTK2
 * GtkGLExt
 * LibJpeg
 * LibPng
 * ZLib

# Compiling

This project uses the usual CMake workflow:

    mkdir build && cd build && cmake .. && make

## linux

```
cmake -G "Unix Makefiles" ..
cmake --build . -- -j$(nproc)
```

## msys2

`base-devel`

### 32 bit:

```
pacman -S mingw-w64-i686-{toolchain,cmake}
pacman -S mingw-w64-i686-{gtk2,gtkglext}
mkdir build && cd build
cmake -G "MSYS Makefiles" .. -DGTK2_GLIBCONFIG_INCLUDE_DIR=/mingw32/lib/glib-2.0/include -DGTK2_GDKCONFIG_INCLUDE_DIR=/mingw32/lib/gtk-2.0/include
cmake --build . -- -j$(nproc)
```

### 64 bit:

```
pacman -S mingw-w64-x86_64-{toolchain,cmake}
pacman -S mingw-w64-x86_64-{gtk2,gtkglext}
mkdir build && cd build
cmake -G "MSYS Makefiles" .. -DGTK2_GLIBCONFIG_INCLUDE_DIR=/mingw64/lib/glib-2.0/include -DGTK2_GDKCONFIG_INCLUDE_DIR=/mingw64/lib/gtk-2.0/include
cmake --build . -- -j$(nproc)
```

More Compilation Details
------------------------

options:
 * `DOWNLOAD_GAMEPACKS=ON`
   Automatically download the gamepack data during the first compilation
 * `RADIANT_ABOUTMSG="Custom build"`
   A message shown in the about dialog

targets:
 * `radiant`    Compiles the radiant core binary
 * `plugins`    Compiles all plugins (each plugin has its own target as well)
 * `modules`    Compiles all modules (each module has its own target as well)
 * `game_packs` Downloads the game pack data
 * `quake3`     Compiles all the Quake3 tools
   - `q3map2`    Quake3 map compiler
   - `q3data`
 * `quake2`     Compiles all the Quake2 tools (Note: needs to be compiled explicitly)
   - `q2map`     Quake2 map compiler
   - `q2data`
   - `h2data`
