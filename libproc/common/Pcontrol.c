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
#include <sys/fault.h>
#include <sys/wait.h>

#include <mutex.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "Putil.h"

int	_libproc_debug;		/* set non-zero to enable debugging printfs */
int	_libproc_no_qsort;	/* set non-zero to inhibit sorting */
				/* of symbol tables */
int	_libproc_incore_elf;	/* only use in-core elf data */

sigset_t blockable_sigs;	/* signals to block when we need to be safe */
static	int	minfd;	/* minimum file descriptor returned by dupfd(fd, 0) */
char	procfs_path[PATH_MAX] = "/proc";

/*
 * Read/write interface for live processes: just pread/pwrite the
 * /proc/<pid>/as file:
 */

static ssize_t
Pread_live(struct ps_prochandle *P, void *buf, size_t n, uintptr_t addr)
{
	return (pread(P->asfd, buf, n, (off_t)addr));
}

static ssize_t
Pwrite_live(struct ps_prochandle *P, const void *buf, size_t n, uintptr_t addr)
{
	return (pwrite(P->asfd, buf, n, (off_t)addr));
}

static const ps_rwops_t P_live_ops = { Pread_live, Pwrite_live };

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

	(void) sigfillset(&blockable_sigs);
	(void) sigdelset(&blockable_sigs, SIGKILL);
	(void) sigdelset(&blockable_sigs, SIGSTOP);
}

void
Pset_procfs_path(const char *path)
{
	(void) snprintf(procfs_path, sizeof (procfs_path), "%s", path);
}

/*
 * Call set_minfd() once before calling dupfd() several times.
 * We assume that the application will not reduce its current file
 * descriptor limit lower than 512 once it has set at least that value.
 */
int
set_minfd(void)
{
	static mutex_t minfd_lock = DEFAULTMUTEX;
	struct rlimit rlim;
	int fd;

	if ((fd = minfd) < 256) {
		(void) mutex_lock(&minfd_lock);
		if ((fd = minfd) < 256) {
			if (getrlimit(RLIMIT_NOFILE, &rlim) != 0)
				rlim.rlim_cur = rlim.rlim_max = 0;
			if (rlim.rlim_cur >= 512)
				fd = 256;
			else if ((fd = rlim.rlim_cur / 2) < 3)
				fd = 3;
/*			membar_producer();*/
			minfd = fd;
		}
		(void) mutex_unlock(&minfd_lock);
	}
	return (fd);
}

int
dupfd(int fd, int dfd)
{
	int mfd;

	/*
	 * Make fd be greater than 255 (the 32-bit stdio limit),
	 * or at least make it greater than 2 so that the
	 * program will work when spawned by init(1m).
	 * Also, if dfd is non-zero, dup the fd to be dfd.
	 */
	if ((mfd = minfd) == 0)
		mfd = set_minfd();
	if (dfd > 0 || (0 <= fd && fd < mfd)) {
		if (dfd <= 0)
			dfd = mfd;
		dfd = fcntl(fd, F_DUPFD, dfd);
		(void) close(fd);
		fd = dfd;
	}
	/*
	 * Mark it close-on-exec so any created process doesn't inherit it.
	 */
	if (fd >= 0)
		(void) fcntl(fd, F_SETFD, FD_CLOEXEC);
	return (fd);
}

/*
 * Create a new controlled process.
 * Leave it stopped on successful exit from exec() or execve().
 * Return an opaque pointer to its process control structure.
 * Return NULL if process cannot be created (fork()/exec() not successful).
 */
struct ps_prochandle *
Pxcreate(
	const char *file,	/* executable file name */
	char *const *argv,	/* argument vector */
	char *const *envp,	/* environment */
	int *perr,	/* pointer to error return code */
	char *path,	/* if non-null, holds exec path name on return */
	size_t len)	/* size of the path buffer */
{
	char execpath[PATH_MAX];
	char procname[PATH_MAX];
	struct ps_prochandle *P;
	pid_t pid;
	int fd;
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

		/*
		 * This is ugly.  There is no execvep() function that takes a
		 * path and an environment.  We cheat here by replacing the
		 * global 'environ' variable right before we call this.
		 */
		if (envp)
			environ = (char **)envp;

		(void) execvp(file, argv);  /* execute the program */
		_exit(127);
	}

	/*
	 * Initialize the process structure.
	 */
	(void) memset(P, 0, sizeof (*P));
	(void) mutex_init(&P->proc_lock, USYNC_THREAD, NULL);
	P->flags |= CREATED;
	P->state = PS_RUN;
	P->pid = pid;
	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;
	P->ops = &P_live_ops;
	Pinitsym(P);

	/*
	 * Get the path to /proc/$pid.
	 */

	snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)pid);
	fname = procname + strlen(procname);

	/*
	 * Exclusive write open advises others not to interfere.
	 * There is no reason for any of these open()s to fail.
	 */
	strcpy(fname, "mem");
/*	if ((fd = open(procname, (O_RDWR|O_EXCL))) < 0 || */
	if ((fd = open(procname, (O_RDONLY|O_EXCL))) < 0 ||
	 /* Changed to O_RDONLY on Linux */
	    (fd = dupfd(fd, 0)) < 0) {
		_dprintf("Pcreate: failed to open %s: %s\n",
		    procname, strerror(errno));
		rc = C_STRANGE;
		goto bad;
	}
	P->asfd = fd;

	fd = -1;
	P->ctlfd = fd;
	
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

        P->status.pr_flags |= PR_STOPPED;
        P->state = PS_STOP;
        P->status.pr_pid = pid;
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

struct ps_prochandle *
Pcreate(
	const char *file,	/* executable file name */
	char *const *argv,	/* argument vector */
	int *perr,	/* pointer to error return code */
	char *path,	/* if non-null, holds exec path name on return */
	size_t len)	/* size of the path buffer */
{
	return (Pxcreate(file, argv, NULL, perr, path, len));
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
 * Grab an existing process.
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * flags:
 *	PGRAB_RETAIN	Retain tracing flags (default clears all tracing flags).
 *	PGRAB_FORCE	Grab regardless of whether process is already traced.
 *	PGRAB_RDONLY	Open the address space file O_RDONLY instead of O_RDWR,
 *                      and do not open the process control file.
 *	PGRAB_NOSTOP	Open the process but do not force it to stop.
 * perr:	pointer to error return code.
 */
struct ps_prochandle *
Pgrab(pid_t pid, int flags, int *perr)
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
	P->ctlfd = -1;
        P->asfd = -1;
        P->statfd = -1;
        P->agentctlfd = -1;
        P->agentstatfd = -1;
	P->status.pr_pid = pid;

	if (ptrace(PT_ATTACH, pid, NULL, 0) != 0) {	
		goto err;
	} else if (waitpid(pid, &status, 0) == -1) {
		goto err;
	} else if (WIFSTOPPED(status) == 0) {
		goto err;
	} else
		P->state = PS_STOP;
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
	case G_BADLWPS:
		str = "bad lwp specification";
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
	uint_t i;

	if (P->core != NULL) {
		lwp_info_t *nlwp, *lwp = list_next(&P->core->core_lwp_head);

		for (i = 0; i < P->core->core_nlwp; i++, lwp = nlwp) {
			nlwp = list_next(lwp);
#ifdef __sparc
			if (lwp->lwp_gwins != NULL)
				free(lwp->lwp_gwins);
			if (lwp->lwp_xregs != NULL)
				free(lwp->lwp_xregs);
			if (lwp->lwp_asrs != NULL)
				free(lwp->lwp_asrs);
#endif
			free(lwp);
		}

		if (P->core->core_platform != NULL)
			free(P->core->core_platform);
		if (P->core->core_uts != NULL)
			free(P->core->core_uts);
		if (P->core->core_cred != NULL)
			free(P->core->core_cred);
#if defined(__i386) || defined(__amd64)
		if (P->core->core_ldt != NULL)
			free(P->core->core_ldt);
#endif

		free(P->core);
	}

	if (P->ucaddrs != NULL) {
		free(P->ucaddrs);
		P->ucaddrs = NULL;
		P->ucnelems = 0;
	}

	(void) mutex_lock(&P->proc_lock);
	if (P->hashtab != NULL) {
#if 0
		struct ps_lwphandle *L;
		for (i = 0; i < HASHSIZE; i++) {
			while ((L = P->hashtab[i]) != NULL)
				Lfree_internal(P, L);
		}
		free(P->hashtab);
#endif
		return; /* It would not happen at Linux. */
	}
	(void) mutex_unlock(&P->proc_lock);
	(void) mutex_destroy(&P->proc_lock);

	if (P->asfd >= 0)
		(void) close(P->asfd);
	Preset_maps(P);

	/* clear out the structure as a precaution against reuse */
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;

	free(P);
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
 * Return the open address space file descriptor for the process.
 * Clients must not close this file descriptor, not use it
 * after the process is freed.
 */
int
Pasfd(struct ps_prochandle *P)
{
	return (P->asfd);
}

/*
 * Return the open control file descriptor for the process.
 * Clients must not close this file descriptor, not use it
 * after the process is freed.
 */
int
Pctlfd(struct ps_prochandle *P)
{
	return (P->ctlfd);
}

/*
 * Return a pointer to the process status structure.
 * Clients should not hold on to this pointer indefinitely.
 * It will become invalid on Prelease().
 */
const pstatus_t *
Pstatus(struct ps_prochandle *P)
{
	return (&P->status);
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
		/* All ptrace() traps are unexpected as we don't use ptrace() */
		_dprintf("Pwait: Unexpected ptrace() trap");
		exit (-1);
	case CLD_CONTINUED:
		_dprintf("Pwait: Child got CLD_CONTINUED: this can never happen");
		exit (-1);

	default:	/* corrupted state */
		_dprintf("Pwait: unknown si_code: %li\n", info.si_code);
		errno = EINVAL;
		return (-1);
	}
	
	return (0);
}

int
Psetrun(struct ps_prochandle *P,
	int sig,	/* signal to pass to process */
	int flags)	/* PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT */
{
	P->info_valid = 0;	/* will need to update map and file info */
	/*
	 * If we've cached ucontext-list information while we were stopped,
	 * free it now.
	 */
	if (P->ucaddrs != NULL) {
		free(P->ucaddrs);
		P->ucaddrs = NULL;
		P->ucnelems = 0;
	}

	if (ptrace (PTRACE_CONT, P->pid, NULL, sig) != 0) {
		_dprintf("Psetrun: %s\n", strerror(errno));
		return (-1);
	}

	P->state = PS_RUN;
	return (0);
}

ssize_t
Pread(struct ps_prochandle *P,
	void *buf,		/* caller's buffer */
	size_t nbyte,		/* number of bytes to read */
	uintptr_t address)	/* address in process */
{
	return (P->ops->p_pread(P, buf, nbyte, address));
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
		if ((nbyte = P->ops->p_pread(P, string, STRSZ, addr)) <= 0) {
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

ssize_t
Pwrite(struct ps_prochandle *P,
	const void *buf,	/* caller's buffer */
	size_t nbyte,		/* number of bytes to write */
	uintptr_t address)	/* address in process */
{
	return (P->ops->p_pwrite(P, buf, nbyte, address));
}

/*
 * Restore original instruction where a breakpoint was set.
 */
int
Pdelbkpt(struct ps_prochandle *P, uintptr_t address, ulong_t saved)
{
	instr_t old = (instr_t)saved;
	instr_t cur;

	if (P->state == PS_DEAD) {
		errno = ENOENT;
		return (-1);
	}

	/*
	 * If the breakpoint instruction we had placed has been overwritten
	 * with a new instruction, then don't try to replace it with the
	 * old instruction. Doing do can cause problems with self-modifying
	 * code -- PLTs for example. If the Pread() fails, we assume that we
	 * should proceed though most likely the Pwrite() will also fail.
	 */
	if (Pread(P, &cur, sizeof (cur), address) == sizeof (cur) &&
	    cur != BPT)
		return (0);

	if (Pwrite(P, &old, sizeof (old), address) != sizeof (old))
		return (-1);

	return (0);
}

core_content_t
Pcontent(struct ps_prochandle *P)
{
	if (P->state == PS_DEAD)
		return (P->core->core_content);

	return (CC_CONTENT_ALL);
}

