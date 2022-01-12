#!/bin/bash
# Oracle Linux DTrace.
# Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Determine if glibc version is at least 2.34.
read VERSMAJOR VERSMINOR <<< `ldd --version | awk '/^ldd/ {print $NF}' | tr '.' ' '`
if [ $VERSMAJOR -lt 2 ]; then
        VERS234=0
elif [ $VERSMAJOR -gt 2 ]; then
        VERS234=1
elif [ $VERSMINOR -lt 34 ]; then
        VERS234=0
else
        VERS234=1
fi

# If we get to a frame we do not recognize, but we saw "main", exit.
# Rename the __libc_start_main frame, depending on glibc version.
awk '
BEGIN { saw_main = 0 }
!/myfunc_/ && !/main/ && saw_main { exit(0) }
  '$VERS234' { sub("__libc_start_call_main", "XXXSTART_MAINXXX") }
! '$VERS234' { sub("__libc_start_main",      "XXXSTART_MAINXXX") }
{ print }
/main/ { saw_main = 1 }'
