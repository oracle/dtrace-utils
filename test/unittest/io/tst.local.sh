#!/bin/bash

#
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Test the io:::start probe for write and read operations by creating
# a file and reading it back after clearing the caches.
#

dtrace=$1
testdir="$(dirname $_test)"
nblocks=1024
filesize=$((1024*$nblocks))
minsize=$((filesize / 10 * 9))
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
mount=`df --output=source $iodir | tail -1`
statname=`basename $mount`

$dtrace $dt_flags -c "test/triggers/doio.sh $tempfile $filesize test/triggers/io-mount-local.sh $iodir $fstype $fsoptions" -qs /dev/stdin <<EODTRACE
BEGIN
{
	byteswr = 0;
	bytesrd = 0;
}

io:::start
/(args[0]->b_flags & B_WRITE) != 0 && args[1]->dev_statname == "$statname"/
{
	byteswr += args[0]->b_bufsize;
}

io:::start
/(args[0]->b_flags & B_WRITE) == 0 && args[1]->dev_statname == "$statname"/
{
        bytesrd += args[0]->b_bufsize;
}

END
/byteswr >= $minsize/
{
	printf("wrote-expected: yes\n");
}

END
/byteswr < $minsize/
{
	printf("wrote-expected: no (%d / %d)\n", byteswr, $minsize);
}

END
/bytesrd >= $minsize/
{
	printf("read-expected: yes\n");
}

END
/bytesrd < $minsize/
{
	printf("read-expected: no (%d / %d)\n", bytesrd, $minsize);
}


EODTRACE

