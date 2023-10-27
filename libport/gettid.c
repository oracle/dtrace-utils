/*
 * Oracle Linux DTrace; get the kernel-level ID of the current thread.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/syscall.h>
#include <unistd.h>

/*
 * This is only used if glibc does not provide an implementation.
 *
 * The kernel always has an implementation of this function.
 */

pid_t gettid(void)
{
#ifdef __NR_gettid
	return syscall(__NR_gettid);
#else
#error The kernel does not define gettid(), and glibc is too old to implement it.
#endif
}
