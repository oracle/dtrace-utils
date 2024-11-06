#!/bin/sh

if [[ ! -e $tracefs/available_filter_functions ]]; then
	echo no tracefs found
	exit 1
fi

FUNCS=${tracefs}/available_filter_functions

if ! grep -qw  _raw_read_lock $FUNCS; then
        echo no _raw_read_lock FBT probe due to kernel config
        exit 1
fi

if ! grep -w _raw_write_lock $FUNCS; then
        echo no _raw_write_lock FBT probe due to kernel config
        exit 1
fi

exit 0
