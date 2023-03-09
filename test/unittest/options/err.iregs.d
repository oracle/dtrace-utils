/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* DTrace should recognize when iregs is too great for # of BPF registers */
#pragma D option iregs=9

BEGIN
{
	a = 1;
	trace(a);
	exit(0);
}
