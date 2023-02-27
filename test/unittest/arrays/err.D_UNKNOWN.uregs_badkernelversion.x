#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -lt 5 ]; then
        exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -lt 15 ]; then
        exit 0
fi

echo "no UNKNOWN-uregs problem on newer kernels"
exit 2
