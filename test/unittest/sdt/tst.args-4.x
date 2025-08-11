#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -lt 6 ]; then
	exit 0
fi
if [ $MAJOR -eq 6 -a $MINOR -lt 14 ]; then
	exit 0
fi

echo "tst.args-4.d disabled on kernels >= 6.14"
exit 2
