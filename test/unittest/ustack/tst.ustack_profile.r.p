#!/usr/bin/gawk -f
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# if we get to a frame we do not recognize, but we saw "main", exit
BEGIN { saw_main = 0 }
!/myfunc_/ && !/main/ && saw_main { exit(0) }
{ print }
/main/ { saw_main = 1 }
