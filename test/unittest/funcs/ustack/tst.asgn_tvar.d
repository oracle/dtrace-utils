/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test assignment of ustack() to a TLS variable.
 */

/* @@trigger: ustack-tst-basic */

pid$target:a.out:myfunc_z:entry
{
	self->v = ustack(3);
	printf("%k", self->v);

	exit(0);
}

ERROR
{
	exit(1);
}
