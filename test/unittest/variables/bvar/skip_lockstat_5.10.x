#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
	exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -ge 10 ]; then
	exit 0
fi

echo "lockstat disabled prior to 5.10"
exit 1
