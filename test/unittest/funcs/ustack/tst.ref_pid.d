/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test accessing the 'pid' field of a userspace stack.
 */

/* @@trigger: ustack-tst-basic */

pid$target:a.out:myfunc_z:entry
{
	trace("Expecting ");
	trace($target);
	trace(", got ");
	trace(ustack(7).pid);
}

pid$target:a.out:myfunc_z:entry
/ustack(7).pid == $target/
{
	exit(0);
}

pid$target:a.out:myfunc_z:entry
/ustack(7).pid != $target/
{
	exit(1);
}

ERROR
{
	exit(1);
}
