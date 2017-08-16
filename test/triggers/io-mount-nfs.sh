#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# Script invoked by unit tests to mount a NFS file system
#

if (( $# != 2 )); then
        echo "expected two argument: <mountdir> <serverpath>" >&2
        exit 2
fi

mountdir=$1
serverpath=$2

mount -t nfs -o nfsvers=3 127.0.0.1:$serverpath $mountdir
