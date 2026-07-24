#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION:
#	The io provider's bio devinfo translator derives dev_name from the
#	device model when DTrace runs in a non-initial PID namespace.
#
# @@timeout: 35

. test/unittest/pidns/common.bash

check_dtrace_arg "$@"
dtrace=$1
DIRNAME="$tmpdir/pidns-io.$$.$RANDOM"
mkdir -p "$DIRNAME"
cd "$DIRNAME" || exit 1

iodir="$DIRNAME/mnt"
image="$DIRNAME/io.img"
tempfile="$iodir/file"

cleanup()
{
	umount "$iodir" >/dev/null 2>&1
	rmdir "$iodir" >/dev/null 2>&1
	rm -f "$image"
}
trap cleanup EXIT

dd if=/dev/zero of="$image" bs=1024 count=$((320 * 1024)) status=none || exit 1
mkfs.xfs -q "$image" >/dev/null || exit 1
mkdir "$iodir" || exit 1
mount -t xfs -o loop,defaults,atime,diratime,nosuid,nodev "$image" "$iodir" || exit 1

devnam=`losetup -j "$image" | gawk 'BEGIN { FS = ":" } ; {print $1}'`
statname=`basename "$devnam"`
if [ -z "$statname" ]; then
	echo "could not determine loopback device"
	exit 1
fi

run_in_pidns "$dtrace" $dt_flags -qn '
io:::start
/args[1]->dev_statname == "'"$statname"'"/
{
	seen++;
}

io:::start
/args[1]->dev_statname == "'"$statname"'" && args[1]->dev_name != "loop"/
{
	printf("expected dev_name loop, got %s\n", args[1]->dev_name);
	bad++;
}

END
/seen == 0/
{
	printf("did not see io:::start for %s\n", "'"$statname"'");
}

END
{
	exit(seen == 0 || bad != 0);
}
' -c "dd if=/dev/zero of=$tempfile bs=4096 count=256 conv=fdatasync status=none"

exit $?
