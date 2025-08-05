/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: profile-tst-ufuncsort */
/* @@nosort */

#pragma D option quiet

pid$target:a.out::return
{
	@[probefunc] = count();
}

tick-2s
{
	exit(0);
}
