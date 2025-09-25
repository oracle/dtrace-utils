/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Assigning a member in ustack() is not allowed.
 */

/* @@trigger: ustack-tst-basic */

pid$target:a.out:myfunc_z:entry
{
	ustack(5).depth = 2;
	exit(0);
}

ERROR
{
	exit(1);
}
