#!/bin/bash

# Skip test if FBT probes do not provide argument datatype info.
types=`$dtrace -lvn fbt::exit_creds:return | gawk '/^[ 	]*args\[/ { $1 = ""; print }' | sort -u`

if [[ -z "$types" ]]; then
	echo "FBT probes without args[] type info"
	exit 2
fi

exit 0
