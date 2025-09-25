/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Corrupted stack data does not cause a crash.
 */

/* @@trigger: periodic_output */

fbt::hrtimer_nanosleep:entry
{
	v = stack(5);
	v.depth = 0x7fffffff;
	printf("%k", v);
	exit(0);
}

ERROR
{
	exit(1);
}
