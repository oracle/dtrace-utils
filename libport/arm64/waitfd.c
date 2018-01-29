/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <config.h>				/* for HAVE_* */

#ifndef HAVE_WAITFD
#include <unistd.h>				/* for syscall() */
#include <platform.h>

int
waitfd(int which, pid_t upid, int options, int flags)
{
        return syscall(__NR_waitfd, which, upid, options, flags);
}

#endif
