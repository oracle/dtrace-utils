/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The address of 'max_pfn' can be obtained.  This exercises the CTF
 *	      data provided by the kernel.
 */

/*
#pragma D option quiet
 */

BEGIN {
	trace(&`max_pfn);
	exit(0);
}
