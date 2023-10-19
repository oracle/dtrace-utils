#!/bin/bash

if [ -r /lib/modules/$(uname -r)/kernel/vmlinux.ctfa ]; then
	exit 0
else
	echo "No CTF archive found for the kernel."
	exit 2
fi
