#!/bin/bash

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
	exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -ge 15 ]; then
	exit 0
fi

echo "uregs[]: pt_regs[] lookup not implemented on older kernels"
exit 1
