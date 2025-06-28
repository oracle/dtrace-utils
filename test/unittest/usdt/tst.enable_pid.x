#!/bin/sh

if [ `grep -c ^processor /proc/cpuinfo` -lt 2 ]; then
	echo test should have at least two processors
	exit 2
fi

exit 0
