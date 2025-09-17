/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/bin/bash

#  SYNOPSIS
#    ./100list_probes.sh
#
#  DESCRIPTION
#    List all probes.  There are many.

sudo /usr/sbin/dtrace -l | head -10
echo "[...]"
sudo /usr/sbin/dtrace -l | tail -10
