/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/bin/bash

#  SYNOPSIS
#    ./102list_probe_arguments.sh
#
#  DESCRIPTION
#    Some providers provide typed arguments for (some of) their
#    probes.  Use "dtrace -lv" for a verbose listing to see the
#    arguments and their types, if any.

sudo /usr/sbin/dtrace -lv -n 'syscall::open:entry'
