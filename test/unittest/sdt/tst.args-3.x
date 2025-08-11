#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -lt 6 ]; then
	echo "tst.args-3.d disabled on kernels < 6.14"
	exit 2
fi
if [ $MAJOR -eq 6 -a $MINOR -lt 14 ]; then
	echo "tst.args-3.d disabled on kernels < 6.14"
	exit 2
fi

exit 0
