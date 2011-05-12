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
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fault.h>
#include <sys/wait.h>

#include <sys/priv.h>
#include <sys/corectl.h>
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
 * Function prototypes for static routines in this module.
 */
static	void	restore_tracing_flags(struct ps_prochandle *);

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
#pragma init(_libproc_init)
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
	int lasterrno = 0;

	if (len == 0)	/* zero length, no path */
		path = NULL;
	if (path != NULL)
		*path = '\0';

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = C_STRANGE;
		return (NULL);
	}

	if ((pid = fork1()) == -1) {
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
		extern void __priv_free_info(void *);
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
		if (P->core->core_priv != NULL)
			free(P->core->core_priv);
/* NEED FIX */ 
/*		if (P->core->core_privinfo != NULL)
			__priv_free_info(P->core->core_privinfo); */
		if (P->core->core_ppii != NULL)
			free(P->core->core_ppii);
		if (P->core->core_zonename != NULL)
			free(P->core->core_zonename);
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
 * Return a pointer to the process psinfo structure.
 * Clients should not hold on to this pointer indefinitely.
 * It will become invalid on Prelease().
 */
const psinfo_t *
Ppsinfo(struct ps_prochandle *P)
{
	if (P->state == PS_IDLE) {
		errno = ENODATA;
		return (NULL);
	}
#if 0
	if (P->state != PS_DEAD && proc_get_psinfo(P->pid, &P->psinfo) == -1)
		return (NULL);
#endif

	return (&P->psinfo);
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
 * Ensure that all cached state is written to the process.
 * The cached state is the LWP's signal mask and registers
 * and the process's tracing flags.
 */
void
Psync(struct ps_prochandle *P)
{
	return;
#if 0
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long cmd[4];
	iovec_t iov[8];
	int n = 0;

	if (P->flags & SETHOLD) {
		cmd[0] = PCSHOLD;
		iov[n].iov_base = (caddr_t)&cmd[0];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_lwp.pr_lwphold;
		iov[n++].iov_len = sizeof (P->status.pr_lwp.pr_lwphold);
	}
	if (P->flags & SETREGS) {
		cmd[1] = PCSREG;
#ifdef __i386
		/* XX64 we should probably restore REG_GS after this */
		if (ctlfd == P->agentctlfd)
			P->status.pr_lwp.pr_reg[GS] = 0;
#elif defined(__amd64)
		/* XX64 */
#endif
		iov[n].iov_base = (caddr_t)&cmd[1];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_lwp.pr_reg[0];
		iov[n++].iov_len = sizeof (P->status.pr_lwp.pr_reg);
	}
	if (P->flags & SETSIG) {
		cmd[2] = PCSTRACE;
		iov[n].iov_base = (caddr_t)&cmd[2];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_sigtrace;
		iov[n++].iov_len = sizeof (P->status.pr_sigtrace);
	}
	if (P->flags & SETFAULT) {
		cmd[3] = PCSFAULT;
		iov[n].iov_base = (caddr_t)&cmd[3];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_flttrace;
		iov[n++].iov_len = sizeof (P->status.pr_flttrace);
	}

/*	if (n == 0 || writev(ctlfd, iov, n) < 0) */
	if (n == 0)
		return;		/* nothing to do or write failed */

	P->flags &= ~(SETSIG|SETFAULT|SETHOLD|SETREGS);
#endif
}

/*
 * Reopen the /proc file (after PS_LOST).
 */
int
Preopen(struct ps_prochandle *P)
{
	int fd;
	char procname[PATH_MAX];
	char *fname;

	if (P->state == PS_DEAD || P->state == PS_IDLE)
		return (0);
#if defined(sun)
	if (P->agentcnt > 0) {
		P->agentcnt = 1;
		Pdestroy_agent(P);
	}
#endif

	(void) snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)P->pid);
	fname = procname + strlen(procname);

/*	(void) strcpy(fname, "as"); */
	(void) strcpy(fname, "mem"); /* for Linux */
	if ((fd = open(procname, O_RDWR)) < 0 ||
	    close(P->asfd) < 0 ||
	    (fd = dupfd(fd, P->asfd)) != P->asfd) {
		_dprintf("Preopen: failed to open %s: %s\n",
		    procname, strerror(errno));
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	P->asfd = fd;


/*	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    close(P->ctlfd) < 0 ||
	    (fd = dupfd(fd, P->ctlfd)) != P->ctlfd) {
		_dprintf("Preopen: failed to open %s: %s\n",
		    procname, strerror(errno));
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}*/
	fd = -1;
	P->ctlfd = fd;

	/*
	 * Set the state to PS_RUN and wait for the process to stop so that
	 * we re-read the status from the new P->statfd.  If this fails, Pwait
	 * will reset the state to PS_LOST and we fail the reopen.  Before
	 * returning, we also forge a bit of P->status to allow the debugger to
	 * see that we are PS_LOST following a successful exec.
	 */
	P->state = PS_RUN;
	if (Pwait(P, 0) == -1) {
#ifdef _ILP32
		if (errno == EOVERFLOW)
			P->status.pr_dmodel = PR_MODEL_LP64;
#endif
		return (-1);
	}

	return (0);
}

/*
 * Define all settable flags other than the microstate accounting flags.
 */
#define	ALL_SETTABLE_FLAGS (PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_PTRACE)

/*
 * Restore /proc tracing flags to their original values
 * in preparation for releasing the process.
 * Also called by Pcreate() to clear all tracing flags.
 */
static void
restore_tracing_flags(struct ps_prochandle *P)
{
	long flags;
	long cmd[4];
	iovec_t iov[4];

	if (P->flags & CREATED) {
		/* we created this process; clear all tracing flags */
		premptyset(&P->status.pr_sigtrace);
		premptyset(&P->status.pr_flttrace);
		if ((P->status.pr_flags & ALL_SETTABLE_FLAGS) != 0)
			(void) Punsetflags(P, ALL_SETTABLE_FLAGS);
	} else {
		/* we grabbed the process; restore its tracing flags */
		P->status.pr_sigtrace = P->orig_status.pr_sigtrace;
		P->status.pr_flttrace = P->orig_status.pr_flttrace;
		if ((P->status.pr_flags & ALL_SETTABLE_FLAGS) !=
		    (flags = (P->orig_status.pr_flags & ALL_SETTABLE_FLAGS))) {
			(void) Punsetflags(P, ALL_SETTABLE_FLAGS);
			if (flags)
				(void) Psetflags(P, flags);
		}
	}

	cmd[0] = PCSTRACE;
	iov[0].iov_base = (caddr_t)&cmd[0];
	iov[0].iov_len = sizeof (long);
	iov[1].iov_base = (caddr_t)&P->status.pr_sigtrace;
	iov[1].iov_len = sizeof (P->status.pr_sigtrace);

	cmd[1] = PCSFAULT;
	iov[2].iov_base = (caddr_t)&cmd[1];
	iov[2].iov_len = sizeof (long);
	iov[3].iov_base = (caddr_t)&P->status.pr_flttrace;
	iov[3].iov_len = sizeof (P->status.pr_flttrace);

	(void) writev(P->ctlfd, iov, 8);

	P->flags &= ~(SETSIG|SETFAULT);
}

/*
 * Release the process.  Frees the process control structure.
 * flags:
 *	PRELEASE_CLEAR	Clear all tracing flags.
 *	PRELEASE_RETAIN	Retain current tracing flags.
 *	PRELEASE_HANG	Leave the process stopped and abandoned.
 *	PRELEASE_KILL	Terminate the process with SIGKILL.
 */
void
Prelease(struct ps_prochandle *P, int flags)
{
	if (P->state == PS_DEAD) {
		_dprintf("Prelease: releasing handle %p PS_DEAD of pid %d\n",
		    (void *)P, (int)P->pid);
		Pfree(P);
		return;
	}

	if (P->state == PS_IDLE) {
		file_info_t *fptr = list_next(&P->file_head);
		_dprintf("Prelease: releasing handle %p PS_IDLE of file %s\n",
		    (void *)P, fptr->file_pname);
		Pfree(P);
		return;
	}

	_dprintf("Prelease: releasing handle %p pid %d\n",
	    (void *)P, (int)P->pid);
	
	ptrace(PTRACE_DETACH, (int)P->pid, 0, 0);

	if (P->ctlfd == -1) {
		Pfree(P);
		return;
	}
#if defined (sun)
	if (P->agentcnt > 0) {
		P->agentcnt = 1;
		Pdestroy_agent(P);
	}
#endif

	/*
	 * Attempt to stop the process.
	 */
	P->state = PS_RUN;
	(void) Pstop(P, 1000);

	if (flags & PRELEASE_KILL) {
		if (P->state == PS_STOP)
			(void) Psetrun(P, SIGKILL, 0);
		(void) kill(P->pid, SIGKILL);
		Pfree(P);
		return;
	}

	/*
	 * We didn't lose control; we do more.
	 */
	Psync(P);

	if (flags & PRELEASE_CLEAR)
		P->flags |= CREATED;

	if (!(flags & PRELEASE_RETAIN))
		restore_tracing_flags(P);

	if (flags & PRELEASE_HANG) {
		/* Leave the process stopped and abandoned */
		(void) Punsetflags(P, PR_RLC|PR_KLC);
		Pfree(P);
		return;
	}

	Pfree(P);
}


/* debugging */
static void
prdump(struct ps_prochandle *P)
{
	uint32_t bits;
#if 0
	prldump("Pstopstatus", &P->status.pr_lwp);
#endif
	bits = *((uint32_t *)&P->status.pr_sigpend);
	if (bits)
		_dprintf("Pstopstatus: pr_sigpend = 0x%.8X\n", bits);
}

/*
 * Wait for the specified process to stop or terminate.
 * Or, just get the current status (PCNULL).
 * Or, direct it to stop and get the current status (PCDSTOP).
 * If the agent LWP exists, do these things to the agent,
 * else do these things to the process as a whole.
 */
int
Pstopstatus(struct ps_prochandle *P,
	long request,		/* PCNULL, PCDSTOP, PCSTOP, PCWSTOP */
	uint_t msec)		/* if non-zero, timeout in milliseconds */
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl[3];
	ssize_t rc;
	int err;
	int old_state = P->state;

	switch (P->state) {
	case PS_RUN:
		break;
	case PS_STOP:
		if (request != PCNULL && request != PCDSTOP)
			return (0);
		break;
	case PS_LOST:
		if (request != PCNULL) {
			errno = EAGAIN;
			return (-1);
		}
		break;
	case PS_UNDEAD:
	case PS_DEAD:
	case PS_IDLE:
		if (request != PCNULL) {
			errno = ENOENT;
			return (-1);
		}
		break;
	default:	/* corrupted state */
		_dprintf("Pstopstatus: corrupted state: %d\n", P->state);
		errno = EINVAL;
		return (-1);
	}

#if 0
	ctl[0] = PCDSTOP;
	ctl[1] = PCTWSTOP;
	ctl[2] = (long)msec;
	rc = 0;
	switch (request) {
	case PCSTOP:
		rc = write(ctlfd, &ctl[0], 3*sizeof (long));
		break;
	case PCWSTOP:
		rc = write(ctlfd, &ctl[1], 2*sizeof (long));
		break;
	case PCDSTOP:
		rc = write(ctlfd, &ctl[0], 1*sizeof (long));
		break;
	case PCNULL:
		if (P->state == PS_DEAD || P->state == PS_IDLE)
			return (0);
		break;
	default:	/* programming error */
		errno = EINVAL;
		return (-1);
	}
	err = (rc < 0)? errno : 0;
#endif
	Psync(P);

#if 0
	if (err) {
		switch (err) {
		case EINTR:		/* user typed ctl-C */
		case ERESTART:
			_dprintf("Pstopstatus: EINTR\n");
			break;
		case EAGAIN:		/* we lost control of the the process */
		case EOVERFLOW:
			_dprintf("Pstopstatus: PS_LOST, errno=%d\n", err);
			P->state = PS_LOST;
			break;
		default:		/* check for dead process */
			if (_libproc_debug) {
				const char *errstr;

				switch (request) {
				case PCNULL:
					errstr = "Pstopstatus PCNULL"; break;
				case PCSTOP:
					errstr = "Pstopstatus PCSTOP"; break;
				case PCDSTOP:
					errstr = "Pstopstatus PCDSTOP"; break;
				case PCWSTOP:
					errstr = "Pstopstatus PCWSTOP"; break;
				default:
					errstr = "Pstopstatus PC???"; break;
				}
				_dprintf("%s: %s\n", errstr, strerror(err));
			}
			deadcheck(P);
			break;
		}
		if (err != EINTR && err != ERESTART) {
			errno = err;
			return (-1);
		}
	}

	if (!(P->status.pr_flags & PR_STOPPED)) {
		P->state = PS_RUN;
		if (request == PCNULL || request == PCDSTOP || msec != 0)
			return (0);
		_dprintf("Pstopstatus: process is not stopped\n");
		errno = EPROTO;
		return (-1);
	}
#endif
	P->state = PS_STOP;

	if (_libproc_debug)	/* debugging */
		prdump(P);

	/*
	 * If the process was already stopped coming into Pstopstatus(),
	 * then don't use its PC to set P->sysaddr since it may have been
	 * changed since the time the process originally stopped.
	 */
	if (old_state == PS_STOP)
		return (0);
#if 0
	switch (P->status.pr_lwp.pr_why) {
	case PR_REQUESTED:
	case PR_SIGNALLED:
	case PR_FAULTED:
	case PR_JOBCONTROL:
	case PR_SUSPENDED:
		break;
	default:
		errno = EPROTO;
		return (-1);
	}
#endif
	return (0);
}

/*
 * Wait for the process to stop for any reason.
 */
int
Pwait(struct ps_prochandle *P, uint_t msec)
{
	return (Pstopstatus(P, PCWSTOP, msec));
}

/*
 * Direct the process to stop; wait for it to stop.
 */
int
Pstop(struct ps_prochandle *P, uint_t msec)
{
	return (Pstopstatus(P, PCSTOP, msec));
}

/*
 * Direct the process to stop; don't wait.
 */
int
Pdstop(struct ps_prochandle *P)
{
	return (Pstopstatus(P, PCDSTOP, 0));
}

int
Psetrun(struct ps_prochandle *P,
	int sig,	/* signal to pass to process */
	int flags)	/* PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT */
{
	Psync(P);	/* flush tracing flags and registers */

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
		dprintf("Psetrun: %s\n", strerror(errno));
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

int
Pclearsig(struct ps_prochandle *P)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl = PCCSIG;

	if (write(ctlfd, &ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);
	P->status.pr_lwp.pr_cursig = 0;
	return (0);
}

int
Pclearfault(struct ps_prochandle *P)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl = PCCFAULT;

	if (write(ctlfd, &ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);
	return (0);
}

/*
 * Set a breakpoint trap, return original instruction.
 */
int
Psetbkpt(struct ps_prochandle *P, uintptr_t address, ulong_t *saved)
{
	long ctl[1 + sizeof (priovec_t) / sizeof (long) +	/* PCREAD */
	    1 + sizeof (priovec_t) / sizeof (long)];	/* PCWRITE */
	long *ctlp = ctl;
	size_t size;
	priovec_t *iovp;
	instr_t bpt = BPT;
	instr_t old;

	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE) {
		errno = ENOENT;
		return (-1);
	}

	/* fetch the old instruction */
	*ctlp++ = PCREAD;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &old;
	iovp->pio_len = sizeof (old);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	/* write the BPT instruction */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &bpt;
	iovp->pio_len = sizeof (bpt);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	size = (char *)ctlp - (char *)ctl;
	if (write(P->ctlfd, ctl, size) != size)
		return (-1);

	/*
	 * Fail if there was already a breakpoint there from another debugger
	 * or DTrace's user-level tracing on x86.
	 */
	if (old == BPT) {
		errno = EBUSY;
		return (-1);
	}

	*saved = (ulong_t)old;
	return (0);
}

/*
 * Restore original instruction where a breakpoint was set.
 */
int
Pdelbkpt(struct ps_prochandle *P, uintptr_t address, ulong_t saved)
{
	instr_t old = (instr_t)saved;
	instr_t cur;

	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE) {
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

/*
 * Common code for Pxecbkpt() and Lxecbkpt().
 * Develop the array of requests that will do the job, then
 * write them to the specified control file descriptor.
 * Return the non-zero errno if the write fails.
 */
static int
execute_bkpt(
	int ctlfd,		/* process or LWP control file descriptor */
	const fltset_t *faultset,	/* current set of traced faults */
	const sigset_t *sigmask,	/* current signal mask */
	uintptr_t address,		/* address of breakpint */
	ulong_t saved)			/* the saved instruction */
{
	long ctl[
	    1 + sizeof (sigset_t) / sizeof (long) +		/* PCSHOLD */
	    1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
	    1 + sizeof (priovec_t) / sizeof (long) +		/* PCWRITE */
	    2 +							/* PCRUN */
	    1 +							/* PCWSTOP */
	    1 +							/* PCCFAULT */
	    1 + sizeof (priovec_t) / sizeof (long) +		/* PCWRITE */
	    1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
	    1 + sizeof (sigset_t) / sizeof (long)];		/* PCSHOLD */
	long *ctlp = ctl;
	sigset_t unblock;
	size_t size;
	ssize_t ssize;
	priovec_t *iovp;
	sigset_t *holdp;
	fltset_t *faultp;
	instr_t old = (instr_t)saved;
	instr_t bpt = BPT;
	int error = 0;

	/* block our signals for the duration */
	(void) sigprocmask(SIG_BLOCK, &blockable_sigs, &unblock);

	/* hold posted signals */
	*ctlp++ = PCSHOLD;
	holdp = (sigset_t *)ctlp;
	prfillset(holdp);
	prdelset(holdp, SIGKILL);
	prdelset(holdp, SIGSTOP);
	ctlp += sizeof (sigset_t) / sizeof (long);

	/* force tracing of FLTTRACE */
	if (!(prismember(faultset, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		faultp = (fltset_t *)ctlp;
		*faultp = *faultset;
		praddset(faultp, FLTTRACE);
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/* restore the old instruction */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &old;
	iovp->pio_len = sizeof (old);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	/* clear current signal and fault; set running w/ single-step */
	*ctlp++ = PCRUN;
	*ctlp++ = PRCSIG | PRCFAULT | PRSTEP;

	/* wait for stop, cancel the fault */
	*ctlp++ = PCWSTOP;
	*ctlp++ = PCCFAULT;

	/* restore the breakpoint trap */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &bpt;
	iovp->pio_len = sizeof (bpt);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	/* restore fault tracing set */
	if (!(prismember(faultset, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		*(fltset_t *)ctlp = *faultset;
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/* restore the hold mask */
	*ctlp++ = PCSHOLD;
	*(sigset_t *)ctlp = *sigmask;
	ctlp += sizeof (sigset_t) / sizeof (long);

	size = (char *)ctlp - (char *)ctl;
	if ((ssize = write(ctlfd, ctl, size)) != size)
		error = (ssize == -1)? errno : EINTR;
	(void) sigprocmask(SIG_SETMASK, &unblock, NULL);
	return (error);
}

/*
 * Step over a breakpoint, i.e., execute the instruction that
 * really belongs at the breakpoint location (the current %pc)
 * and leave the process stopped at the next instruction.
 */
int
Pxecbkpt(struct ps_prochandle *P, ulong_t saved)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	int rv, error;

	if (P->state != PS_STOP) {
		errno = EBUSY;
		return (-1);
	}

	Psync(P);

	error = execute_bkpt(ctlfd,
	    &P->status.pr_flttrace, &P->status.pr_lwp.pr_lwphold,
	    P->status.pr_lwp.pr_reg[R_PC], saved);
	rv = Pstopstatus(P, PCNULL, 0);

	if (error != 0) {
		if (P->status.pr_lwp.pr_why == PR_JOBCONTROL &&
		    error == EBUSY) {	/* jobcontrol stop -- back off */
			P->state = PS_RUN;
			return (0);
		}
		if (error == ENOENT)
			return (0);
		errno = error;
		return (-1);
	}

	return (rv);
}

/*
 * Install the watchpoint described by wp.
 */
int
Psetwapt(struct ps_prochandle *P, const prwatch_t *wp)
{
	long ctl[1 + sizeof (prwatch_t) / sizeof (long)];
	prwatch_t *cwp = (prwatch_t *)&ctl[1];

	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE) {
		errno = ENOENT;
		return (-1);
	}

	ctl[0] = PCWATCH;
	cwp->pr_vaddr = wp->pr_vaddr;
	cwp->pr_size = wp->pr_size;
	cwp->pr_wflags = wp->pr_wflags;

	if (write(P->ctlfd, ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);

	return (0);
}

/*
 * Remove the watchpoint described by wp.
 */
int
Pdelwapt(struct ps_prochandle *P, const prwatch_t *wp)
{
	long ctl[1 + sizeof (prwatch_t) / sizeof (long)];
	prwatch_t *cwp = (prwatch_t *)&ctl[1];

	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE) {
		errno = ENOENT;
		return (-1);
	}

	ctl[0] = PCWATCH;
	cwp->pr_vaddr = wp->pr_vaddr;
	cwp->pr_size = wp->pr_size;
	cwp->pr_wflags = 0;

	if (write(P->ctlfd, ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);

	return (0);
}

/*
 * Common code for Pxecwapt() and Lxecwapt().  Develop the array of requests
 * that will do the job, then write them to the specified control file
 * descriptor.  Return the non-zero errno if the write fails.
 */
static int
execute_wapt(
	int ctlfd,		/* process or LWP control file descriptor */
	const fltset_t *faultset,	/* current set of traced faults */
	const sigset_t *sigmask,	/* current signal mask */
	const prwatch_t *wp)		/* watchpoint descriptor */
{
	long ctl[
	    1 + sizeof (sigset_t) / sizeof (long) +		/* PCSHOLD */
	    1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
	    1 + sizeof (prwatch_t) / sizeof (long) +		/* PCWATCH */
	    2 +							/* PCRUN */
	    1 +							/* PCWSTOP */
	    1 +							/* PCCFAULT */
	    1 + sizeof (prwatch_t) / sizeof (long) +		/* PCWATCH */
	    1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
	    1 + sizeof (sigset_t) / sizeof (long)];		/* PCSHOLD */

	long *ctlp = ctl;
	int error = 0;

	sigset_t unblock;
	sigset_t *holdp;
	fltset_t *faultp;
	prwatch_t *prw;
	ssize_t ssize;
	size_t size;

	(void) sigprocmask(SIG_BLOCK, &blockable_sigs, &unblock);

	/*
	 * Hold all posted signals in the victim process prior to stepping.
	 */
	*ctlp++ = PCSHOLD;
	holdp = (sigset_t *)ctlp;
	prfillset(holdp);
	prdelset(holdp, SIGKILL);
	prdelset(holdp, SIGSTOP);
	ctlp += sizeof (sigset_t) / sizeof (long);

	/*
	 * Force tracing of FLTTRACE since we need to single step.
	 */
	if (!(prismember(faultset, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		faultp = (fltset_t *)ctlp;
		*faultp = *faultset;
		praddset(faultp, FLTTRACE);
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/*
	 * Clear only the current watchpoint by setting pr_wflags to zero.
	 */
	*ctlp++ = PCWATCH;
	prw = (prwatch_t *)ctlp;
	prw->pr_vaddr = wp->pr_vaddr;
	prw->pr_size = wp->pr_size;
	prw->pr_wflags = 0;
	ctlp += sizeof (prwatch_t) / sizeof (long);

	/*
	 * Clear the current signal and fault; set running with single-step.
	 * Then wait for the victim to stop and cancel the FLTTRACE.
	 */
	*ctlp++ = PCRUN;
	*ctlp++ = PRCSIG | PRCFAULT | PRSTEP;
	*ctlp++ = PCWSTOP;
	*ctlp++ = PCCFAULT;

	/*
	 * Restore the current watchpoint.
	 */
	*ctlp++ = PCWATCH;
	(void) memcpy(ctlp, wp, sizeof (prwatch_t));
	ctlp += sizeof (prwatch_t) / sizeof (long);

	/*
	 * Restore fault tracing set if we modified it.
	 */
	if (!(prismember(faultset, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		*(fltset_t *)ctlp = *faultset;
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/*
	 * Restore the hold mask to the current hold mask (i.e. the one
	 * before we executed any of the previous operations).
	 */
	*ctlp++ = PCSHOLD;
	*(sigset_t *)ctlp = *sigmask;
	ctlp += sizeof (sigset_t) / sizeof (long);

	size = (char *)ctlp - (char *)ctl;
	if ((ssize = write(ctlfd, ctl, size)) != size)
		error = (ssize == -1)? errno : EINTR;
	(void) sigprocmask(SIG_SETMASK, &unblock, NULL);
	return (error);
}

/*
 * Step over a watchpoint, i.e., execute the instruction that was stopped by
 * the watchpoint, and then leave the LWP stopped at the next instruction.
 */
int
Pxecwapt(struct ps_prochandle *P, const prwatch_t *wp)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	int rv, error;

	if (P->state != PS_STOP) {
		errno = EBUSY;
		return (-1);
	}

	Psync(P);
	error = execute_wapt(ctlfd,
	    &P->status.pr_flttrace, &P->status.pr_lwp.pr_lwphold, wp);
	rv = Pstopstatus(P, PCNULL, 0);

	if (error != 0) {
		if (P->status.pr_lwp.pr_why == PR_JOBCONTROL &&
		    error == EBUSY) {	/* jobcontrol stop -- back off */
			P->state = PS_RUN;
			return (0);
		}
		if (error == ENOENT)
			return (0);
		errno = error;
		return (-1);
	}

	return (rv);
}

int
Psetflags(struct ps_prochandle *P, long flags)
{
	int rc;
/* Only change P->status but not write to file at Linux. */
/*	long ctl[2];

	ctl[0] = PCSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		rc = -1;
	} else { */
		P->status.pr_flags |= flags;
		rc = 0;
/*	} */

	return (rc);
}

int
Punsetflags(struct ps_prochandle *P, long flags)
{
	int rc;
/*	long ctl[2];

	ctl[0] = PCUNSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		rc = -1;
	} else { */
		P->status.pr_flags &= ~flags;
		rc = 0;
/*	} */

	return (rc);
}

/*
 * Common function to allow clients to manipulate the action to be taken
 * on receipt of a signal, receipt of machine fault, entry to a system call,
 * or exit from a system call.  We make use of our private prset_* functions
 * in order to make this code be common.  The 'which' parameter identifies
 * the code for the event of interest (0 means change the entire set), and
 * the 'stop' parameter is a boolean indicating whether the process should
 * stop when the event of interest occurs.  The previous value is returned
 * to the caller; -1 is returned if an error occurred.
 */
static int
Psetaction(struct ps_prochandle *P, void *sp, size_t size,
    uint_t flag, int max, int which, int stop)
{
	int oldval;

	if (which < 0 || which > max) {
		errno = EINVAL;
		return (-1);
	}

	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE) {
		errno = ENOENT;
		return (-1);
	}

	oldval = prset_ismember(sp, size, which) ? TRUE : FALSE;

	if (stop) {
		if (which == 0) {
			prset_fill(sp, size);
			P->flags |= flag;
		} else if (!oldval) {
			prset_add(sp, size, which);
			P->flags |= flag;
		}
	} else {
		if (which == 0) {
			prset_empty(sp, size);
			P->flags |= flag;
		} else if (oldval) {
			prset_del(sp, size, which);
			P->flags |= flag;
		}
	}

	if (P->state == PS_RUN)
		Psync(P);

	return (oldval);
}

/*
 * Set action on specified signal.
 */
int
Psignal(struct ps_prochandle *P, int which, int stop)
{
	int oldval;

	if (which == SIGKILL && stop != 0) {
		errno = EINVAL;
		return (-1);
	}

	oldval = Psetaction(P, &P->status.pr_sigtrace, sizeof (sigset_t),
	    SETSIG, PRMAXSIG, which, stop);

	if (oldval != -1 && which == 0 && stop != 0)
		prdelset(&P->status.pr_sigtrace, SIGKILL);

	return (oldval);
}

/*
 * Set all signal tracing flags.
 */
void
Psetsignal(struct ps_prochandle *P, const sigset_t *set)
{
	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE)
		return;

	P->status.pr_sigtrace = *set;
	P->flags |= SETSIG;

	if (P->state == PS_RUN)
		Psync(P);
}

/*
 * Set action on specified fault.
 */
int
Pfault(struct ps_prochandle *P, int which, int stop)
{
	return (Psetaction(P, &P->status.pr_flttrace, sizeof (fltset_t),
	    SETFAULT, PRMAXFAULT, which, stop));
}

/*
 * Set all machine fault tracing flags.
 */
void
Psetfault(struct ps_prochandle *P, const fltset_t *set)
{
	if (P->state == PS_DEAD || P->state == PS_UNDEAD ||
	    P->state == PS_IDLE)
		return;

	P->status.pr_flttrace = *set;
	P->flags |= SETFAULT;

	if (P->state == PS_RUN)
		Psync(P);
}

core_content_t
Pcontent(struct ps_prochandle *P)
{
	if (P->state == PS_DEAD)
		return (P->core->core_content);
	if (P->state == PS_IDLE)
		return (CC_CONTENT_TEXT | CC_CONTENT_DATA | CC_CONTENT_CTF);

	return (CC_CONTENT_ALL);
}


/*
 * Add a mapping to the given proc handle.  Resizes the array as appropriate and
 * manages reference counts on the given file_info_t.
 *
 * The 'map_relocate' member is used to tell Psort_mappings() that the
 * associated file_map pointer needs to be relocated after the mappings have
 * been sorted.  It is only set for the first mapping, and has no meaning
 * outside these two functions.
 */
int
Padd_mapping(struct ps_prochandle *P, off64_t off, file_info_t *fp,
    prmap_t *pmap)
{
	map_info_t *mp;

	if (P->map_count == P->map_alloc) {
		size_t next = P->map_alloc ? P->map_alloc * 2 : 16;

		if ((P->mappings = realloc(P->mappings,
		    next * sizeof (map_info_t))) == NULL)
			return (-1);

		P->map_alloc = next;
	}

	mp = &P->mappings[P->map_count++];

	mp->map_offset = off;
	mp->map_pmap = *pmap;
	mp->map_relocate = 0;
	if ((mp->map_file = fp) != NULL) {
		if (fp->file_map == NULL) {
			fp->file_map = mp;
			mp->map_relocate = 1;
		}
		fp->file_ref++;
	}

	return (0);
}

static int
map_sort(const void *a, const void *b)
{
	const map_info_t *ap = a, *bp = b;

	if (ap->map_pmap.pr_vaddr < bp->map_pmap.pr_vaddr)
		return (-1);
	else if (ap->map_pmap.pr_vaddr > bp->map_pmap.pr_vaddr)
		return (1);
	else
		return (0);
}

/*
 * Sort the current set of mappings.  Should be called during target
 * initialization after all calls to Padd_mapping() have been made.
 */
void
Psort_mappings(struct ps_prochandle *P)
{
	int i;
	map_info_t *mp;

	qsort(P->mappings, P->map_count, sizeof (map_info_t), map_sort);

	/*
	 * Update all the file_map pointers to refer to the new locations.
	 */
	for (i = 0; i < P->map_count; i++) {
		mp = &P->mappings[i];
		if (mp->map_relocate)
			mp->map_file->file_map = mp;
		mp->map_relocate = 0;
	}
}
