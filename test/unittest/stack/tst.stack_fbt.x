#!/bin/bash

read MAJOR MINOR <<< `uname -r | awk -F. '{print $1, $2}'`

if [ $MAJOR -eq 5 -a $MINOR -lt 8 ]; then
	exit 0
else
	echo "Function __vfs_write no longer exists starting in 5.8"
	exit 2
fi
