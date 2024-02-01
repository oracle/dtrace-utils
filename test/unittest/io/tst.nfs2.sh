#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Test the io:::start probe for write and read operations by creating
# a file and reading it back after clearing the caches.
#

rundt="$1 $dt_flags -qs $PWD/test/unittest/io/dump_io_probe_args.d -c"
check_args=$PWD/test/unittest/io/check_io_probe_args.sh
retval=0

DIRNAME="$tmpdir/io-nfs2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

filesize=$((1024*1024))

exdir=`mktemp -u`
iodir=`mktemp -u`
tempfile=`mktemp -u -p $iodir`

trap "rm -f $tempfile; umount $iodir; rmdir $iodir; exportfs -u 127.0.0.1:$exdir; rmdir $exdir" QUIT

systemctl enable --now nfs-server > /dev/null 2>&1

mkdir $exdir
  exportfs -i -v -o "rw,sync,no_root_squash,insecure,fsid=8434437288" 127.0.0.1:$exdir > /dev/null
    mkdir $iodir
        mount -t nfs -o nfsvers=3 127.0.0.1:$exdir $iodir
            $rundt "dd if=/dev/urandom of=$tempfile count=$filesize bs=1 status=none" -o log.write
            myinode=`stat $tempfile  | awk '/	Inode: / {print $4}'`
        umount $iodir
        # flush caches and remount to force IO
	echo 3 > /proc/sys/vm/drop_caches
        mount -t nfs -o nfsvers=3 127.0.0.1:$exdir $iodir
            $rundt "sum $tempfile"                                                    -o log.read
            rm -f $tempfile
        umount $iodir
    rmdir $iodir
  exportfs -u 127.0.0.1:$exdir
rmdir $exdir

# check the DTrace output

$check_args log.write
if [ $? -ne 0 ]; then
    cat log.write
    retval=1
fi
$check_args log.read
if [ $? -ne 0 ]; then
    cat log.read
    retval=1
fi

echo myinode is $myinode
cat > awk.txt << EOF
# initialize
BEGIN { err = 0; bytes = 0 }

# skip over uninteresting records
NF == 0 { next }
\$6 != myflags { next }
\$22 != "nfs" { next }

# check
\$4 != "start" &&
\$4 != "done" { print "  ERROR: probe name should be start or done"; err = 1 }
\$4 == "start" { bytes += \$7 }
\$12 != "$myinode" { print "  ERROR: blknode should be inode"; err = 1 }
\$14 != 0 { print "  ERROR: iodone should be 0"; err = 1 }
\$21 != "nfs" { print "  ERROR: name should be nfs"; err = 1 }
END {
      if (bytes < $filesize || bytes > $filesize + spill) {
          print "  ERROR: total bytes should match filesize (allow spill)", bytes, $filesize, spill;
          err = 1;
      }
      exit(err);
}
EOF

echo check start bytes in log.write
awk -v myflags=520 -v spill=4095 -f awk.txt log.write
if [ $? -ne 0 ]; then
    echo "  ERROR: post-processing error log.write"
    cat log.write
    retval=1
fi

echo check start bytes in log.read
awk -v myflags=460 -v spill=0 -f awk.txt log.read
if [ $? -ne 0 ]; then
    echo "  ERROR: post-processing error log.read"
    cat log.read
    retval=1
fi

exit $retval
