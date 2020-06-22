#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

# @@timeout: 70

##
#
# ASSERTION:
# SDT probes correctly handle probes in modules that contain text sections
# other than .text.
#
##

pound_disk() {
    while :; do
        mkdir $tmpdir/ext4/blah
        cp /bin/ls $tmpdir/ext4/blah
        dd if=/dev/zero of=$tmpdir/ext4/womble bs=1024 seek=$(($RANDOM*1024)) count=$RANDOM
        sync
        cat $tmpdir/ext4/blah/ls > $tmpdir/ext4/womble
        rm -r /$tmpdir/ext4/blah
    done
}

mkdir $tmpdir/ext4
dd if=/dev/zero of=$tmpdir/disk.img bs=$((1024*1024)) seek=100 count=1
mkfs.ext4 -F $tmpdir/disk.img
trap "umount -l $tmpdir/ext4; rm $tmpdir/disk.img" QUIT EXIT
mount -o loop -t ext4 -o defaults,atime,diratime,nosuid,nodev $tmpdir/disk.img $tmpdir/ext4

pound_disk &
pounder=$!

dtrace=$1

$dtrace $dt_flags -qm 'perf:ext4 { @probes[probename] = count(); } ' -n 'tick-1s { i++; }' -n 'tick-1s / i > 50 / { exit(0); }'

kill -9 $pounder
wait 2>/dev/null

exit 0
