#!/bin/sh

# set -ex

INSTALLDIR=`pwd`/install

if [[ `file $INSTALLDIR/radiant.exe` == *"x86-64"* ]]; then
    MINGWDIR=/mingw64
else
    MINGWDIR=/mingw32
fi

function dependencies_single_target_no_depth {
    local TARGET=$1

    local DEPENDENCIESFILTER="| grep 'DLL Name' | sed -r 's/\s+DLL\s+Name\:\s+//' | xargs -i{} which {} | grep $MINGWDIR/bin"
    local COMMAND="objdump -x $TARGET $DEPENDENCIESFILTER | xargs -i{} echo {}"

    local DEPENDENCIES=`eval "$COMMAND"`

    if [ "$DEPENDENCIES" != "" ]; then
        echo "$DEPENDENCIES"
    fi
}

function dependencies {
    local TARGETS=$@

    local TEMPORARYFILEA="install-dlls-msys2-mingw.alldependencies.tmp"
    local TEMPORARYFILEB="install-dlls-msys2-mingw.dependencies.tmp"

    local ALLDEPENDENCIES=""

    for TARGET in $TARGETS; do
        local ALLDEPENDENCIES=`dependencies_single_target_no_depth "$TARGET" && echo "$ALLDEPENDENCIES"`
    done

    local ALLDEPENDENCIES=`echo "$ALLDEPENDENCIES" | sort -u`

    local NEWDEPENDENCIES="$ALLDEPENDENCIES"

    while [ "$NEWDEPENDENCIES" != "" ]; do
        local DEPENDENCIES=""

        for DEPENDENCY in $NEWDEPENDENCIES; do
            DEPENDENCIES=`dependencies_single_target_no_depth "$DEPENDENCY" && echo "$DEPENDENCIES"`
        done

        echo "$ALLDEPENDENCIES" > "$TEMPORARYFILEA"
        echo "$DEPENDENCIES" | sort -u > "$TEMPORARYFILEB"

        local NEWDEPENDENCIES=`comm -13 "$TEMPORARYFILEA" "$TEMPORARYFILEB"`

        if [ "$NEWDEPENDENCIES" != "" ]; then
            local ALLDEPENDENCIES=`printf '%s\n' "$ALLDEPENDENCIES" "$NEWDEPENDENCIES" | sort`
        fi

        rm "$TEMPORARYFILEA" "$TEMPORARYFILEB"
    done

    if [ "$ALLDEPENDENCIES" != "" ]; then
        echo "$ALLDEPENDENCIES"
    fi
}

for DEPENDENCY in `dependencies ./install/*.exe`; do
    cp -v "$DEPENDENCY" "$INSTALLDIR"
done

cd $MINGWDIR

for EXTRAPATH in \
    './lib/gtk-2.0/2.10.0/engines/*.dll' \
    './lib/gtk-2.0/modules/*.dll' \
    './share/themes' \
; do
    cp --parent -v `find $EXTRAPATH -type f` "$INSTALLDIR"
done
