#!/bin/bash

if [ ! -r include/dtrace/dif_defines.h ]; then
	exit 2
fi

exit 0
