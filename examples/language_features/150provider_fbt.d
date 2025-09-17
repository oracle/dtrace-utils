/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./150provider_fbt.d
 *
 *  DESCRIPTION
 *    Function boundary tracing (fbt) can be used to trace the
 *    beginning and end of just about any function in the kernel.
 *    As such, it is very powerful.
 *
 *    That said, there are some challenges:
 *
 *      - One must know the kernel fairly well.  There may
 *        be tens of thousands of functions available to trace.
 *
 *      - Functions that can be traced will vary from one
 *        kernel build to another.  For example, a function
 *        might be inlined in a kernel build, making that
 *        function not traceable in that build.
 *
 *    Also, there are both:
 *
 *      - an fbt provider, which supports typed probe arguments
 *        (seen with "dtrace -lvP fbt")
 *
 *      - a rawfbt provider, whose probe arguments are not typed
 *        nor specially ordered;  traceable functions include
 *        compiler-generated optimized variants of functions,
 *        named <func>.<suffix>.
 */

/* probe on entry to kernel function ksys_write() */
fbt::ksys_write:entry
{
	printf("write %d bytes to fd %d\n", args[2], args[0]);
}

/* do the same thing with a rawfbt probe, so the args are not typed */
rawfbt::ksys_write:entry
{
	printf("write %d bytes to fd %d\n", arg2, arg0);
}

fbt::ksys_write:return
{
	exit(0);
}
