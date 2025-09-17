/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/bin/bash

#  SYNOPSIS
#    ./170provider_lockstat.sh
#
#  DESCRIPTION
#    The lockstat provider can be used to study lock usage.
#    For example, here we count how many times spin locks
#    and adaptive locks are acquired by the command "date".

sudo /usr/sbin/dtrace -qn '
lockstat:::spin-acquire,
lockstat:::adaptive-acquire
/pid == $target/
{
	@locks[probename] = count();
}' -c date
