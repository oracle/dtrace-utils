#!/bin/bash

#
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
#

#
# Script invoked by unit tests to mount a local file system
#

if (( $# != 3 )); then
        echo "expected 3 arguments: <mountdir> <fstype> [<fsoptions>]" >&2
        exit 2
fi

mountdir=$1
fstype=$2
fsoptions=${3-""}

mount -t $fstype -o loop,$fsoptions $mountdir.img $mountdir
