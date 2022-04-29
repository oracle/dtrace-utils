#!/usr/bin/gawk -f
# Oracle Linux DTrace.
# Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Rename the __libc_start_main frame to some standard form.
# Once we get to a libc.so frame, exit.
{ sub("__libc_start.*_main", "XXXSTART_MAINXXX") }
{ print }
/libc\.so/ { exit(0) }
