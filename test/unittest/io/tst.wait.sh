#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Test the io:::wait-start and io:::wait-done probes.
#
# @@xfail: dtv2

dtrace=$1
nblocks=1024
filesize=$((1024*$nblocks))
fstype=xfs
# file system-specific options
fsoptions="defaults,atime,diratime,nosuid,nodev"
iodir=$tmpdir/test-$fstype-io
tempfile=`mktemp -u -p $iodir`

trap "umount $iodir; rmdir $iodir; rm -f $iodir.img" QUIT EXIT

# create loopback file system
dd if=/dev/zero of=$iodir.img bs=1024 count=$((16*$nblocks)) status=none
mkfs.$fstype $iodir.img > /dev/null
mkdir $iodir
test/triggers/io-mount-local.sh $iodir $fstype $fsoptions

# determine the statname
mount=`losetup -j $iodir.img | awk 'BEGIN { FS = ":" } ; {print $1}'`
statname=`basename $mount`

$dtrace $dt_flags -c "test/triggers/doio.sh $tempfile $filesize test/triggers/io-mount-local.sh $iodir $fstype $fsoptions" -qs /dev/stdin <<EODTRACE
io:::wait-start
/(args[0]->b_flags & B_WRITE) != 0 && args[1]->dev_statname == "$statname" /
{
	wait_write_start++;
	self->wait_write_started = 1;
}

io:::wait-done
/self->wait_write_started && (args[0]->b_flags & B_WRITE) != 0 && 
	args[1]->dev_statname == "$statname"/
{
	wait_write_done++;
	self->wait_write_started = 0;
}

io:::wait-start
/(args[0]->b_flags & B_READ) != 0 && args[1]->dev_statname == "$statname" /
{
	wait_read_start++;
	self->wait_read_started = 1;
}

io:::wait-done
/self->wait_read_started && (args[0]->b_flags & B_READ) != 0 && 
	args[1]->dev_statname == "$statname"/
{
	wait_read_done++;
	self->wait_read_started = 0;
}

END
/(wait_write_start > 0) && (wait_write_start == wait_write_done)/
{
	printf("wait-write-expected: yes\n");
}

END
/(wait_write_start == 0) || (wait_write_start != wait_write_done)/
{
	printf("wait-write-expected: no (%d / %d)\n", wait_write_start,
	    wait_write_done);
}

END
/(wait_read_start > 0) && (wait_read_start == wait_read_done)/
{
	printf("wait-read-expected: yes\n");
}

END
/(wait_read_start == 0) || (wait_read_start != wait_read_done)/
{
	printf("wait-read-expected: no (%d / %d)\n", wait_read_start,
	    wait_read_done);
}

EODTRACE
