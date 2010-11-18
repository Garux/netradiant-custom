#!/bin/sh

basecommit=`git log --pretty=format:%H --grep=::zerowing-base`
baserev=`git log --pretty=format:%s --grep=::zerowing-base | cut -d = -f 2 | cut -d ' ' -f 1`

newbaserev=`svn info https://zerowing.idsoftware.com/svn/radiant/GtkRadiant/ | grep ^Revision | cut -d ' ' -f 2`

echo "$baserev -> $newbaserev"

merge()
{
	if ! svn diff -r"$baserev":"$newbaserev" "$1" | git apply --reject --ignore-space-change --directory="$2"; then
		if find . -name \*.rej | grep .; then
			echo "you have to fix these conflicts"
			bash
			echo "succeeded?"
			read -r LINE
			if [ x"$LINE" != x"y" ]; then
				exit 1
			fi
		fi
	fi
}

# GtkRadiant 1.5 changes
git clean  -xdf

# ZeroRadiant's q3map2 changes
merge https://zerowing.idsoftware.com/svn/radiant/GtkRadiant/branches/1.5 /
merge https://zerowing.idsoftware.com/svn/radiant/GtkRadiant/trunk/tools/quake3 tools/quake3
merge https://zerowing.idsoftware.com/svn/radiant/GtkRadiant/trunk/libs/picomodel libs/picomodel

git commit -a -m"::zerowing-base=$newbaserev" -e
