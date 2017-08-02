#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Script invoked by unit tests to generate IO
#

if (( $# < 4 )); then
        echo "expected 5 or more arguments: <tempfile> <filesize> <mountcmd> <mountdir> [<mountarg1>] [<mountarg2>]" >&2
        exit 2
fi

tempfile=$1
filesize=$2
mountcmd=$3
mountdir=$4
mountarg1=${5-""}
mountarg2=${6-""}

set -e

# do writes
dd if=/dev/urandom of=$tempfile count=$filesize bs=1 status=none

# remount the file system to flush the associated caches and force the IO
umount $mountdir
$mountcmd $mountdir $mountarg1 $mountarg2

# do reads
sum $tempfile > /dev/null
