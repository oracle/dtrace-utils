/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Assigning a member in stack() is not allowed.
 */

/* @@trigger: periodic_output */

fbt::hrtimer_nanosleep:entry
{
	stack(5).depth = 2;
	exit(0);
}

ERROR
{
	exit(1);
}
