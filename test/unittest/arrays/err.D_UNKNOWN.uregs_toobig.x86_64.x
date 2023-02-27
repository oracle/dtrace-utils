#!/bin/sh

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -lt 5 ]; then
	exit 2
fi
if [ $MAJOR -eq 5 -a $MINOR -lt 15 ]; then
	exit 2
fi

[ `uname -m` = "x86_64" ] && exit 0
exit 2
