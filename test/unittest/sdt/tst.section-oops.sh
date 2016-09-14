#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2016 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
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
trap "umount $tmpdir/ext4; rm $tmpdir/disk.img" QUIT EXIT
mount -o loop -t ext4 -o defaults,atime,diratime,nosuid,nodev $tmpdir/disk.img $tmpdir/ext4

pound_disk &
pounder=$!

dtrace=$1

$dtrace $dt_flags -qm 'perf:ext4 { @probes[probename] = count(); } ' -n 'tick-1s { i++; }' -n 'tick-1s / i > 50 / { exit(0); }'

kill -9 $pounder

exit 0
