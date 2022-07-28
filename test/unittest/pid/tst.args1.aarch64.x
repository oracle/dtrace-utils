#!/bin/sh

read MAJOR MINOR <<< `uname -r | grep -Eo '^[0-9]+\.[0-9]+' | tr '.' ' '`

if [ $MAJOR -gt 5 ]; then
	exit 0
fi
if [ $MAJOR -eq 5 -a $MINOR -gt 4 ]; then
	exit 0
fi

# Technically, Linux 5.4 should also work, but there seem to be problems with
# UEK6.

echo "pid entry probes have bad arg8/arg9 on pre-5.5 kernels on ARM"
exit 1
