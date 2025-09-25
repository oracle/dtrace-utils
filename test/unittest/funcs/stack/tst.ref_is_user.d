/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test accessing the 'is_user' field of a kernel stack.
 */

/* @@trigger: periodic_output */

fbt::hrtimer_nanosleep:entry
{
	trace("Expecting 0, got ");
	trace(stack(7).is_user);
}

fbt::hrtimer_nanosleep:entry
/stack(7).is_user == 0/
{
	exit(0);
}

fbt::hrtimer_nanosleep:entry
/stack(7).is_user != 0/
{
	exit(1);
}

ERROR
{
	exit(1);
}
