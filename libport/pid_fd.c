/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <port.h>				/* for waitfd, etc */
#ifdef HAVE_SYS_PIDFD_H
#include <sys/pidfd.h>
#endif

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

#ifndef PIDFD_THREAD
/* An arbitrary value unlikely ever to be useful for pidfds.  */
#define PIDFD_THREAD O_NOCTTY
#endif

int
pid_fd(pid_t pid)
{
	int fd;
#ifdef HAVE_SYS_PIDFD_H
	if ((fd = pidfd_open(pid, PIDFD_THREAD)) >= 0)
		return fd;
#else
	if ((fd = syscall(__NR_pidfd_open, pid, PIDFD_THREAD)) >= 0)
		return fd;
#endif

	/*
	 * WEXITED | WSTOPPED is what Pwait() waits for.
	 */
        return waitfd(P_PID, pid, WEXITED | WSTOPPED, 0);
}
