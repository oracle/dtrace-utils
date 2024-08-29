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

rundt="$1 $dt_flags -qs $PWD/test/unittest/io/dump_io_probe_args.d"
check_args=$PWD/test/unittest/io/check_io_probe_args.sh
retval=0

DIRNAME="$tmpdir/io-local2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

filesize=$((512*1024))

fsoptions="loop,defaults,atime,diratime,nosuid,nodev"
iodir=`mktemp -u`
tempfile=`mktemp -u -p $iodir`

trap "rm -f $tempfile; umount $iodir; rmdir $iodir; rm -f $iodir.img" QUIT

# We will trace while bytes are written, which could be during "dd",
# asynchronously with kworker threads, or flushed during umount.
cat << EOF > write.sh
#!/bin/sh
dd if=/dev/urandom of=$tempfile count=$filesize bs=1 status=none
umount $iodir
EOF
chmod +x write.sh

dd if=/dev/zero of=$iodir.img bs=1024 count=$((300*1024)) status=none
mkfs.xfs $iodir.img > /dev/null
    mkdir $iodir
        mount -t xfs -o $fsoptions $iodir.img $iodir
            devnam=`losetup -j $iodir.img | gawk 'BEGIN { FS = ":" } ; {print $1}'`
            statname=`basename $devnam`
            $rundt -o log.write -c ./write.sh

        mount -t xfs -o $fsoptions $iodir.img $iodir
            $rundt -c "sum $tempfile" -o log.read
            rm -f $tempfile
        umount $iodir
    rmdir $iodir
rm -f $iodir.img

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

echo statname is $statname
cat > awk.txt << EOF
# initialize
BEGIN { err = 0; bytes = 0; nrec = 0 }

# skip over uninteresting records
NF == 0 { next }
\$4 == "wait-start" { next }
\$4 == "wait-done" { next }
\$14 != myiodone { next }
\$22 != "$statname" { next }

# check
\$4 != "start" &&
\$4 != "done" { print "  ERROR: probe name should be start or done"; err = 1 }
\$6 != myflags { print "  ERROR: flags are wrong"; err = 1 }
\$4 == "start" { bytes += \$7; nrec++ }
\$21 != "loop" { print "  ERROR: name is wrong"; err = 1 }
END {
      if (bytes != $filesize) {
          print "  ERROR: total bytes should match filesize", bytes, $filesize;
          err = 1;
      }
      if (nrecflag == 1 && nrec != 1) {
          print "expected one record";
          err = 1;
      }
      exit(err);
}
EOF

myaddr=`gawk '$3 == "xfs_end_bio"       {print $1}' /proc/kallsyms`
echo check start bytes in log.write with xfs_end_bio address $myaddr
gawk -v myflags=520 -v nrecflag=1 -v myiodone=$myaddr -f awk.txt log.write
if [ $? -ne 0 ]; then
    echo "  ERROR: post-processing error log.write"
    cat log.write
    retval=1
fi

myaddr=`gawk '$3 == "iomap_read_end_io" {print $1}' /proc/kallsyms`
echo check start bytes in log.read with iomap_read_end_io address $myaddr
gawk -v myflags=460 -v nrecflag=2 -v myiodone=$myaddr -f awk.txt log.read
if [ $? -ne 0 ]; then
    echo "  ERROR: post-processing error log.read"
    cat log.read
    retval=1
fi
exit $retval
