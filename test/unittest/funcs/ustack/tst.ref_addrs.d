/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test accessing the 'addrs' array of a userspace stack.
 */

/* @@trigger: ustack-tst-basic */

pid$target:a.out:myfunc_z:entry
{
	trace("Expecting ustack-tst-basic`myfunc_z, got ");
	ufunc(ustack(7).addrs[0]);
	exit(0);
}

ERROR
{
	exit(1);
}
