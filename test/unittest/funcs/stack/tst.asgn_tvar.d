/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test assignment of stack() to a TLS variable.
 */

/* @@trigger: periodic_output */

fbt::hrtimer_nanosleep:entry
{
	self->v = stack(3);
	printf("%k", self->v);

	exit(0);
}

ERROR
{
	exit(1);
}
