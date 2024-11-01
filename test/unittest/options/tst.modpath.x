#!/bin/bash

# Skip test if there is no CTF archive.
if [ ! -r /lib/modules/$(uname -r)/kernel/vmlinux.ctfa ]; then
	echo "No CTF archive found for the kernel."
	exit 2
fi

# Skip test if CTF info is not used for rawtp args[] types.  (If all rawtp
# args[] types are "uint64_t", this is a symptom of our using the back-up
# trial-and-error method.)
types=`$dtrace $dt_flags -lvP rawtp | gawk '/^[ 	]*args/ { $1 = ""; print }' | sort -u`
if [ "$types" == " uint64_t" ]; then
	echo "not using CTF for rawtp args types"
	exit 2
fi

exit 0
