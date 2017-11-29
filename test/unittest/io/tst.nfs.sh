#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Test the io:::start probe for write and read operations by creating
# a file and reading it back after clearing the caches.
#

dtrace=$1
filesize=$((1024*1024))
minsize=$((filesize / 10 * 9))

serverpath=`mktemp -u`
clientpath=`mktemp -u`
tempfile=`mktemp -u -p $clientpath`
statname="nfs"

trap "rm -f $tempfile; umount $clientpath; rmdir $clientpath; exportfs -u 127.0.0.1:$serverpath; rmdir $serverpath" QUIT EXIT

# setup NFS server
service nfs start > /dev/null 2>&1
mkdir $serverpath
exportfs -i -v -o "rw,sync,no_root_squash,insecure,fsid=8434437287" 127.0.0.1:$serverpath > /dev/null

# setup NFS client
mkdir $clientpath
test/triggers/io-mount-nfs.sh $clientpath $serverpath

$dtrace $dt_flags -c "test/triggers/doio.sh $tempfile $filesize test/triggers/io-mount-nfs.sh $clientpath $serverpath" -qs /dev/stdin <<EODTRACE
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
