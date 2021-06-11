/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <config.h>				/* for HAVE_* */

#ifndef HAVE_PIDFD_OPEN
#include <unistd.h>				/* for syscall() */
#include <port.h>				/* possibly for __NR_pidfd_open */

int pidfd_open (pid_t pid, unsigned int flags)
{
	return syscall(__NR_pidfd_open, pid, flags);
}
#endif
