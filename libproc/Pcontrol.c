/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Portions Copyright 2007 Chad Mynhier
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <mutex.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "Putil.h"

int	_libproc_debug;		/* set non-zero to enable debugging printfs */
int	_libproc_no_qsort;	/* set non-zero to inhibit sorting */
				/* of symbol tables */
int	_libproc_incore_elf;	/* only use in-core elf data */

char	procfs_path[PATH_MAX] = "/proc";

/*
 * This is the library's .init handler.
 */
_dt_constructor_(_libproc_init)
void
_libproc_init(void)
{
	_libproc_debug = getenv("LIBPROC_DEBUG") != NULL;
	_libproc_no_qsort = getenv("LIBPROC_NO_QSORT") != NULL;
	_libproc_incore_elf = getenv("LIBPROC_INCORE_ELF") != NULL;
}

/*
 * Create a new controlled process.
 * Leave it stopped on successful exit from exec() or execve().
 * Return an opaque pointer to its process control structure.
 * Return NULL if process cannot be created (fork()/exec() not successful).
 */
struct ps_prochandle *
Pcreate(
	const char *file,	/* executable file name */
	char *const *argv,	/* argument vector */
	int *perr,	/* pointer to error return code */
	char *path,	/* if non-null, holds exec path name on return */
	size_t len)	/* size of the path buffer */
{
	char execpath[PATH_MAX];
	char procname[PATH_MAX];
	struct ps_prochandle *P;
	pid_t pid;
	char *fname;
	int rc;
	int status;

	if (len == 0)	/* zero length, no path */
		path = NULL;
	if (path != NULL)
		*path = '\0';

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = C_STRANGE;
		return (NULL);
	}

	if ((pid = fork()) == -1) {
		free(P);
		*perr = C_FORK;
		return (NULL);
	}

	if (pid == 0) {			/* child process */
		id_t id;
		extern char **environ;

		ptrace(PTRACE_TRACEME, 0, NULL, NULL);

		/*
		 * If running setuid or setgid, reset credentials to normal.
		 */
		if ((id = getgid()) != getegid())
			(void) setgid(id);
		if ((id = getuid()) != geteuid())
			(void) setuid(id);

		(void) execvp(file, argv);  /* execute the program */
		_exit(127);
	}

	/*
	 * Initialize the process structure.
	 */
	(void) memset(P, 0, sizeof (*P));
	P->state = PS_RUN;
	P->pid = pid;
	Pinitsym(P);
	(void) Pmemfd(P);			/* populate ->memfd */

	/*
	 * Get the path to /proc/$pid.
	 */

	snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)pid);
	fname = procname + strlen(procname);

	waitpid (pid, &status, 0);
	if (WIFEXITED(status)) {
		rc = C_NOENT;
		_dprintf ("Pcreate: exec of %s failed: %s\n",
		    procname, strerror(errno));
		goto bad;
	}

	strcpy(fname, "exe");
	if (readlink(procname, execpath, PATH_MAX) < 0) {
		_dprintf("Pcreate: executable readlink failed: %s\n", strerror(errno));
		rc = C_STRANGE;
		goto bad;
	}

        *perr = 0;
        return P;

bad:
	(void) kill(pid, SIGKILL);
	if (path != NULL && rc != C_PERM && rc != C_LP64)
		*path = '\0';
	Pfree(P);
	*perr = rc;
	return (NULL);
}

/*
 * Return a printable string corresponding to a Pcreate() error return.
 */
const char *
Pcreate_error(int error)
{
	const char *str;

	switch (error) {
	case C_FORK:
		str = "cannot fork";
		break;
	case C_PERM:
		str = "file is set-id or unreadable";
		break;
	case C_NOEXEC:
		str = "cannot execute file";
		break;
	case C_INTR:
		str = "operation interrupted";
		break;
	case C_LP64:
		str = "program is _LP64, self is not";
		break;
	case C_STRANGE:
		str = "unanticipated system error";
		break;
	case C_FDS:
		str = "out of file descriptors";
		break;
	case C_NOENT:
		str = "cannot find executable file";
		break;
	default:
		str = "unknown error";
		break;
	}

	return (str);
}

/*
 * Grab an existing process.  Try to force it to stop (but failure at this is
 * not an error, except if we manage to ptrace() but not stop).
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * perr:	pointer to error return code.
 */
struct ps_prochandle *
Pgrab(pid_t pid, int *perr)
{
	struct ps_prochandle *P;
	int status;

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = G_STRANGE;
		return (NULL);
        }
	(void) memset(P, 0, sizeof (*P));
	P->state = PS_RUN;
	P->pid = pid;
	(void) Pmemfd(P);			/* populate ->memfd */

	if (ptrace(PT_ATTACH, pid, NULL, 0) != 0) {
		/* Not an error */
	} else if (waitpid(pid, &status, 0) == -1) {
		goto err;
	} else if (WIFSTOPPED(status) == 0) {
		goto err;
	} else
		P->state = PS_TRACESTOP;
	return P;
err:
	*perr = errno;
	Pfree (P);
	return NULL;
}

/*
 * Return a printable string corresponding to a Pgrab() error return.
 */
const char *
Pgrab_error(int error)
{
	const char *str;

	switch (error) {
	case G_NOPROC:
		str = "no such process";
		break;
	case G_NOCORE:
		str = "no such core file";
		break;
	case G_NOPROCORCORE:
		str = "no such process or core file";
		break;
	case G_NOEXEC:
		str = "cannot find executable file";
		break;
	case G_ZOMB:
		str = "zombie process";
		break;
	case G_PERM:
		str = "permission denied";
		break;
	case G_BUSY:
		str = "process is traced";
		break;
	case G_SYS:
		str = "system process";
		break;
	case G_SELF:
		str = "attempt to grab self";
		break;
	case G_INTR:
		str = "operation interrupted";
		break;
	case G_LP64:
		str = "program is _LP64, self is not";
		break;
	case G_FORMAT:
		str = "file is not an ELF core file";
		break;
	case G_ELF:
		str = "libelf error";
		break;
	case G_NOTE:
		str = "core file is corrupt or missing required data";
		break;
	case G_STRANGE:
		str = "unanticipated system error";
		break;
	case G_ISAINVAL:
		str = "wrong ELF machine type";
		break;
	case G_NOFD:
		str = "too many open files";
		break;
	default:
		str = "unknown error";
		break;
	}

	return (str);
}

/*
 * Free a process control structure.
 * Close the file descriptors but don't do the Prelease logic.
 */
void
Pfree(struct ps_prochandle *P)
{
	(void) Pclose(P);
	Preset_maps(P);

	/* clear out the structure as a precaution against reuse */
	(void) memset(P, 0, sizeof (*P));

	free(P);
}

/*
 * Close the process's cached file descriptors.
 *
 * They are reopened when needed.
 */
void
Pclose(struct ps_prochandle *P)
{
	if (P->memfd > -1) {
		(void) close(P->memfd);
		P->memfd = -1;
	}
}

/*
 * Return true if this process has any fds open.
 */
int
Phasfds(struct ps_prochandle *P)
{
	return (P->memfd > -1);
}

/*
 * Return the state of the process, one of the PS_* values.
 */
int
Pstate(struct ps_prochandle *P)
{
	return (P->state);
}

/*
 * Return the open memory file descriptor for the process, reopening it if
 * needed.
 *
 * Clients must not close this file descriptor, not use it after the process is
 * freed or Pclose()d.
 */
int
Pmemfd(struct ps_prochandle *P)
{
	char procname[PATH_MAX];
	char *fname;

	if (P->memfd != -1)
		return (P->memfd);

	/*
	 * Get the path to /proc/$pid.
	 */

	snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)P->pid);
	fname = procname + strlen(procname);

	(void) Pmemfd(P);			/* populate ->memfd */
	strcpy(fname, "mem");
	if ((P->memfd = open(procname, (O_RDONLY|O_EXCL))) < 0) {
		_dprintf("Pcreate: failed to open %s: %s\n",
		    procname, strerror(errno));
		exit(-1);
	}

	(void) fcntl(P->memfd, F_SETFD, FD_CLOEXEC);
	return (P->memfd);
}

/*
 * Release the process.  Frees the process control structure.
 * If kill_it is set, kill the process instead.
 */
void
Prelease(struct ps_prochandle *P, boolean_t kill_it)
{
	if (P->state == PS_DEAD) {
		_dprintf("Prelease: releasing handle %p PS_DEAD of pid %d\n",
		    (void *)P, (int)P->pid);
		Pfree(P);
		return;
	}

	_dprintf("Prelease: releasing handle %p pid %d\n",
	    (void *)P, (int)P->pid);

	if (kill_it)
		ptrace(PTRACE_KILL, (int)P->pid, 0, 0);
	else
		ptrace(PTRACE_DETACH, (int)P->pid, 0, 0);

	Pfree(P);
}

/*
 * Wait for the process to stop for any reason.
 */
int
Pwait(struct ps_prochandle *P, uint_t msec)
{
	int err;
	siginfo_t info;

	errno = 0;
	do
	{
		err = waitid(P_PID, P->pid, &info, WEXITED | WSTOPPED);

		if (err == 0)
			break;

		if (errno == ECHILD) {
			P->state = PS_DEAD;
			return (0);
		}

		if (errno != EINTR) {
			_dprintf("Pwait: error waiting: %s", strerror(errno));
			return (-1);
		}
	} while (errno == EINTR);

	switch (info.si_code) {
	case CLD_EXITED:
	case CLD_KILLED:
	case CLD_DUMPED:
		P->state = PS_DEAD;
		break;
	case CLD_STOPPED:
		P->state = PS_STOP;
		break;
	case CLD_TRAPPED:
		/* All ptrace() traps are unexpected as we don't use ptrace()
		   except at initial process connection time. */
		_dprintf("Pwait: Unexpected ptrace() trap");
		exit(-1);
	case CLD_CONTINUED:
		_dprintf("Pwait: Child got CLD_CONTINUED: this can never happen");
		exit(-1);

	default:	/* corrupted state */
		_dprintf("Pwait: unknown si_code: %li\n", (long int) info.si_code);
		errno = EINVAL;
		return (-1);
	}
	
	return (0);
}

int
Psetrun(struct ps_prochandle *P)
{
	P->info_valid = 0;	/* will need to update map and file info */

	if (P->state == PS_TRACESTOP) {
		if (ptrace(PTRACE_DETACH, P->pid, NULL, 0) != 0) {
			_dprintf("Psetrun: %s\n", strerror(errno));
			return (-1);
		}
		P->state = PS_RUN;
	}

	return (0);
}

ssize_t
Pread(struct ps_prochandle *P,
	void *buf,		/* caller's buffer */
	size_t nbyte,		/* number of bytes to read */
	uintptr_t address)	/* address in process */
{
	return (pread(Pmemfd(P), buf, nbyte, (off_t)address));
}

ssize_t
Pread_string(struct ps_prochandle *P,
	char *buf, 		/* caller's buffer */
	size_t size,		/* upper limit on bytes to read */
	uintptr_t addr)		/* address in process */
{
	enum { STRSZ = 40 };
	char string[STRSZ + 1];
	ssize_t leng = 0;
	int nbyte;

	if (size < 2) {
		errno = EINVAL;
		return (-1);
	}

	size--;			/* ensure trailing null fits in buffer */

	*buf = '\0';
	string[STRSZ] = '\0';

	for (nbyte = STRSZ; nbyte == STRSZ && leng < size; addr += STRSZ) {
		if ((nbyte = Pread(P, string, STRSZ, addr)) <= 0) {
			buf[leng] = '\0';
			return (leng ? leng : -1);
		}
		if ((nbyte = strlen(string)) > 0) {
			if (leng + nbyte > size)
				nbyte = size - leng;
			(void) strncpy(buf + leng, string, nbyte);
			leng += nbyte;
		}
	}
	buf[leng] = '\0';
	return (leng);
}

core_content_t
Pcontent(struct ps_prochandle *P)
{
	return (CC_CONTENT_ALL);
}

