#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright © 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

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
