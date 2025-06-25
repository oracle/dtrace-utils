#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Check that 'stackdepth' is consistent with stack().

dtrace=$1
nblocks=1024
filesize=$((1024*$nblocks))
fsoptions="defaults,atime,diratime,nosuid,nodev"
iodir=$tmpdir/tst-stackdepth-io.$$
tempfile=`mktemp -u -p $iodir`

trap "umount $iodir; rmdir $iodir; rm -f $iodir.img" QUIT EXIT

# create loopback file system
dd if=/dev/zero of=$iodir.img bs=1024 count=$((300*$nblocks)) status=none
mkfs.xfs $iodir.img > /dev/null
mkdir $iodir
test/triggers/io-mount-local.sh $iodir xfs $fsoptions

$dtrace $dt_flags -c "test/triggers/doio.sh $tempfile $filesize test/triggers/io-mount-local.sh $iodir xfs $fsoptions" -qn '
io:::
{
	printf("DEPTH %d\n", stackdepth);
	printf("TRACE BEGIN\n");
	stack(100);
	printf("TRACE END\n");
	exit(0);
}

ERROR
{
	printf("error encountered\n");
	exit(1);
}'

exit $?
