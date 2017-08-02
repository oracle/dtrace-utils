/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C */
/* @@no-xfail */

/*
 * Test that there is no value of 'size' which can be passed to copyin
 * to cause mischief.  The somewhat odd order of operations ensures
 * that we test both size = 0 and size = 0xfff...fff
 */
#include <sys/types.h>


#if defined(_LP64)
#define MAX_BITS 63
size_t size;
#else
#define MAX_BITS 31
size_t size;
#endif

syscall:::
/pid == $pid/
{
	printf("size = 0x%lx\n", (ulong_t)size);
}

syscall:::
/pid == $pid/
{
	tracemem(copyin((uintptr_t)curthread->dtrace_psinfo->envp, size), 10);
}

syscall:::
/pid == $pid && size > (1 << MAX_BITS)/
{
	exit(0);
}

syscall:::
/pid == $pid/
{
	size = (size << 1ULL) | 1ULL;
}
