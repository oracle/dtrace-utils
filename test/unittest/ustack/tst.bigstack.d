/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-bigstack */
/* @@tags: unstable */

/* @@skip: fails intermittently, not testing anything useful? */

syscall::ioctl:entry
/pid == $target/
{
	@[ustackdepth] = count();
}

ERROR
/arg4 == DTRACEFLT_BADSTACK/
{
	exit(0);
}

profile:::tick-1s
/++n  == 10/
{
	exit(1)
}
