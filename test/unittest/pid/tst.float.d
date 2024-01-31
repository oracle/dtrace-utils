/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-float */
/* @@trigger-timing: before */

/*
 * ASSERTION: Make sure we can work on processes that use the FPU
 *
 * SECTION: pid provider
 */

pid$1:a.out:main:
{
	@[probename] = count();
}

profile:::tick-5s
{
	exit(0);
}

