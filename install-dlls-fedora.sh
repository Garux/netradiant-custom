#!/bin/sh
set -x

DLL_PATH=/usr/i686-w64-mingw32/sys-root/mingw/bin

cd install

STARTDIR=`pwd`

COPYDEPS() 
{
	FILE=$1
	if [ -e "$FILE" ] ;then
		DEPS=`objdump -p "$FILE" 2>/dev/null |grep -i "DLL Name"|sort |uniq|cut -d\  -f3 |egrep -vi '(GDI32.dll|KERNEL32.dll|USER32.dll|msvcrt.dll|MSIMG32.DLL|ole32.dll|OPENGL32.DLL|SHELL32.DLL|WS2_32.dll)' || true`
				
		for DEP in $DEPS; do
			basename -a "$STARTDIR"/*.dll | grep -v "*.dll"|sort >  "$STARTDIR"/.HAVES
			if ! cat "$STARTDIR"/.HAVES | grep "$DEP" >/dev/null ;then
				cp -v "$DLL_PATH"/"$DEP" "$STARTDIR" 2>/dev/null
				COPYDEPS "$DLL_PATH"/"$DEP"
			fi
			rm -f "$STARTDIR"/.HAVES
		done
	fi
}

for i in *.exe;do
	COPYDEPS "$i";
done

