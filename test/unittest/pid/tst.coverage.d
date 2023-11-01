/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-args1 */
/* @@trigger-timing: before */

/*
 * ASSERTION: test that we can trace some instructions safely
 *
 * SECTION: pid provider
 */

pid$1:a.out::
{
	n++;
}

profile:::tick-2sec
{
	exit(n > 0 ? 0 : 1);
}

ERROR
{
	exit(1);
}
