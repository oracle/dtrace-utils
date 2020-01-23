/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: testprobe */
/* @@runtest-opts: -C */

/*
 * Ensure that arguments to direct-call probes can be retrieved both from
 * registers and the stack.
 */
dt_test:::test
/i++ == 0/
{
#ifndef __aarch64__
	printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
#else
	printf("%d, %d, %d, %d, %d, %d, %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6);
#endif
	exit(0);
}
