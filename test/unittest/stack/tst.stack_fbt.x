#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+'

if [ $MAJOR -eq 5 -a $MINOR -lt 8 ]; then
	exit 0
else
	echo "Function __vfs_write no longer exists starting in 5.8"
	exit 2
fi
