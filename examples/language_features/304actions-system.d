/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

#pragma D option quiet
#pragma D option destructive

/*
 *  SYNOPSIS
 *    sudo ./304actions-system.d
 *
 *  DESCRIPTION
 *    One can launch system calls from within D script,
 *    but this must be explicitly allowed with the "destructive"
 *    option or dtrace -w switch.
 */

BEGIN
{
	system("ls");
	system("echo hello world");

	exit(0);
}
