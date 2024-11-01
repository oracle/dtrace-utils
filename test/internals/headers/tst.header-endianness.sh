#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@tags: uninstalled

set -e

dtrace=$1
CC=/usr/bin/gcc
OLDDIRNAME=${PWD}
CFLAGS="-I${OLDDIRNAME}/include -I${OLDDIRNAME}/uts/common"

DIRNAME="$tmpdir/usdt-header-endianness.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

for header in ${OLDDIRNAME}/include/dtrace/*.h; do
    grep -q _ENDIAN $header || continue
    cat > test.c <<EOF
    #include "$header"
    #if !defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
    #error $header: needs to include <sys/dtrace_types.h>
    #endif
EOF
    $CC $test_cppflags -c $CFLAGS -o /dev/null test.c
done
