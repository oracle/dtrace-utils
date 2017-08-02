/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * Portions Copyright 2007 Chad Mynhier
 */

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
#include <assert.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <dt_debug.h>

#include <mutex.h>
#include <platform.h>
#include <port.h>

#include "Pcontrol.h"
#include "libproc.h"

char	procfs_path[PATH_MAX] = "/proc";

static	int Pgrabbing = 0; 	/* A Pgrab() is underway. */

static int systemd_system = -1; /* 1 if this is a system running systemd. */

static void Pfree_internal(struct ps_prochandle *P);
static void ignored_child_wait(struct ps_prochandle *P, pid_t pid,
    void (*fun)(struct ps_prochandle *P, pid_t pid));
static void ignored_child_flush(struct ps_prochandle *P, pid_t pid);
static prev_states_t *Ppush_state(struct ps_prochandle *P, int state);
static int Ppop_state(struct ps_prochandle *P);
static int Pwait_handle_waitpid(struct ps_prochandle *P, int status);
static int bkpt_handle(struct ps_prochandle *P, uintptr_t addr);
static int bkpt_handle_start(struct ps_prochandle *P, bkpt_t *bkpt);
static int bkpt_handle_post_singlestep(struct ps_prochandle *P, bkpt_t *bkpt);
static int Pbkpt_continue_internal(struct ps_prochandle *P, bkpt_t *bkpt,
	int singlestep);
static void Punbkpt_child_poke(struct ps_prochandle *P, pid_t pid, bkpt_t *bkpt);
static void bkpt_flush(struct ps_prochandle *P, pid_t pid, int gone);
static bkpt_t *bkpt_by_addr(struct ps_prochandle *P, uintptr_t addr,
    int delete);
static int add_bkpt(struct ps_prochandle *P, uintptr_t addr,
    int after_singlestep, int notifier,
    int (*bkpt_handler) (uintptr_t addr, void *data),
    void (*bkpt_cleanup) (void *data),
    void *data);
static void delete_bkpt_handler(struct bkpt *bkpt);
static jmp_buf **single_thread_unwinder_pad(struct ps_prochandle *unused);

static ptrace_lock_hook_fun *ptrace_lock_hook;
libproc_unwinder_pad_fun *libproc_unwinder_pad = single_thread_unwinder_pad;

#define LIBPROC_PTRACE_OPTIONS PTRACE_O_TRACEEXEC | \
	PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | \
	PTRACE_O_TRACEEXIT

_dt_printflike_(1,2)
void
_dprintf(const char *format, ...)
{
	va_list alist;

	if (!_dtrace_debug)
		return;

	va_start(alist, format);
	dt_debug_printf("libproc", format, alist);
	va_end(alist);
}

/*
 * Single-threaded unwinder pad, used if no other pad is registered.
 */
static __thread jmp_buf *default_unwinder_pad;

static jmp_buf **
single_thread_unwinder_pad(struct ps_prochandle *unused)
{
	return &default_unwinder_pad;
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
	void *wrap_arg,		/* args for hooks and wrappers */
	int *perr)		/* pointer to error return code */
{
	struct ps_prochandle *P;
	char procname[PATH_MAX];
	pid_t pid;
	int rc;
	int status;
	int forkblock[2];

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = ENOMEM;
		return (NULL);
	}

	memset(P, 0, sizeof (*P));
	P->bkpts = calloc(BKPT_HASH_BUCKETS, sizeof (struct bkpt_t *));
	if (!P->bkpts) {
		_dprintf("Out of memory initializing breakpoint hash\n");
		*perr = ENOMEM;
		free(P);
		return (NULL);
	}

	P->wrap_arg = wrap_arg;
	Pset_ptrace_wrapper(P, NULL);
	Pset_pwait_wrapper(P, NULL);
	if (pipe(forkblock) < 0) {
		*perr = errno;
		_dprintf("Pcreate: out of fds forking %s\n", file);
		free(P);
		return (NULL);
        }

	if ((pid = fork()) == -1) {
		free(P);
		close(forkblock[0]);
		close(forkblock[1]);
		*perr = EAGAIN;
		return (NULL);
	}

	if (pid == 0) {			/* child process */
		id_t id;
		int dummy;

		/*
		 * If running setuid or setgid, reset credentials to normal,
		 * then wait for our parent to ptrace us.
		 */
		if ((id = getgid()) != getegid())
			(void) setgid(id);
		if ((id = getuid()) != geteuid())
			(void) setuid(id);

		close(forkblock[1]);
		read(forkblock[0], &dummy, 1);
		close(forkblock[0]);
		(void) execvp(file, argv);  /* execute the program */
		_exit(127);
	}
	close(forkblock[0]);

	/*
	 * Initialize the process structure.  Because we have ptrace()d
	 * explicitly, without using Ptrace(), we must update the ptrace count
	 * and related state, and call the lock hook ourselves.
	 */
	P->state = PS_TRACESTOP;
	P->ptrace_count++;
	P->ptraced = TRUE;
	P->ptrace_halted = TRUE;
	P->noninvasive = FALSE;
	P->pid = pid;
	if (Ppush_state(P, PS_RUN) == NULL) {
		rc = errno;
		_dprintf("Pcreate: error pushing state: %s\n", strerror(errno));
		goto bad;
	}
	if (ptrace_lock_hook)
		ptrace_lock_hook(P, P->wrap_arg, 1);

	if (Psym_init(P) < 0) {
		rc = errno;
		_dprintf("Pcreate: error initializing symbol table: %s\n",
		    strerror(errno));
		goto bad;
	}

	/*
	 * ptrace() the process with our tracing options active, and unblock it.
	 */
	if (wrapped_ptrace(P, PTRACE_SEIZE, pid, 0, LIBPROC_PTRACE_OPTIONS |
		    PTRACE_O_TRACECLONE) < 0) {
		rc = errno;
		_dprintf("Pcreate: seize of %s failed: %s\n", file,
		    strerror(errno));
		goto bad;
	}
	close(forkblock[1]);

	waitpid(pid, &status, 0);
	if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP ||
	    (status >> 8) != (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {
		rc = ENOENT;
		_dprintf ("Pcreate: exec of %s failed\n", file);
		goto bad_untrace;
	}

	/*
	 * Initialize the memfd, now we have exec()ed.  Leave the mapfilefd unset
	 * until needed.
	 */
	P->mapfilefd = -1;
	P->memfd = -1;
	if (Pmemfd(P) == -1)	{		/* populate ->memfd */
		/* error already reported */
		rc = errno;
		goto bad_untrace;
	}

	/*
	 * The process is now stopped, waiting in execve() until resumed.
	 */

	snprintf(procname, sizeof (procname), "%s/%d/exe",
	    procfs_path, P->pid);

	if ((Pread_isa_info(P, procname)) < 0) {
		/* error already reported */
		rc = errno;
		goto bad_untrace;
	}

	_dprintf("Pcreate: forked off PID %i from %s\n", P->pid, file);

	*perr = 0;
        return P;

bad_untrace:
	Puntrace(P, 0);
bad:
	(void) kill(pid, SIGKILL);
	*perr = rc;
	if (ptrace_lock_hook)
		ptrace_lock_hook(P, P->wrap_arg, 0);
	Pfree_internal(P);
	return (NULL);
}

/*
 * Grab an existing process.  Try to force it to stop (but failure at this is
 * not an error, except if we manage to ptrace() but not stop).
 *
 * By default, upon Puntrace(), the process will be set running again, but not
 * released.  Use Ptrace_set_detached() to change this behaviour, or Prelease() to
 * unconditionally release.
 *
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * noninvasiveness: if 1, inability to ptrace() is not an error; if 2, do a
 *                  noninvasive grab unconditionally
 * already_ptraced: if 1, this process is already in ptrace() trace-stop state
 * wrap_arg:	arg for hooks and wrappers
 * perr:	pointer to error return code.
 */
struct ps_prochandle *
Pgrab(pid_t pid, int noninvasiveness, int already_ptraced, void *wrap_arg,
    int *perr)
{
	struct ps_prochandle *P;
	char procname[PATH_MAX];

	*perr = 0;
	if (kill(pid, 0) == ESRCH) {
		*perr = errno;
		return (NULL);
	}

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = ENOMEM;
		return (NULL);
        }
	memset(P, 0, sizeof (*P));
	P->state = already_ptraced ? PS_TRACESTOP : PS_RUN;
	P->pid = pid;
	P->detach = 1;
	if (Psym_init(P) < 0)
		goto bad;

	P->bkpts = calloc(BKPT_HASH_BUCKETS, sizeof (struct bkpt_t *));
	if (!P->bkpts)
		goto bad;
	P->wrap_arg = wrap_arg;
	Pset_ptrace_wrapper(P, NULL);
	Pset_pwait_wrapper(P, NULL);

	P->memfd = -1;
	P->mapfilefd = -1;

	if (noninvasiveness < 2) {

		/*
		 * Populate ->memfd as a side-effect.
		 */
		if (Pmemfd(P) == -1) {
			if (noninvasiveness < 1)
				/* error already reported */
				goto bad;
			else
				noninvasiveness = 2;
		} else if (!already_ptraced) {
			/*
			 * Pmemfd() grabbed, try to ptrace().
			 */
			Pgrabbing = 1;
			*perr = Ptrace(P, 1);
			Pgrabbing = 0;

			if (*perr < 0) {
				if (noninvasiveness < 1) {
					Pfree_internal(P);
					return NULL;
				}
				close(P->memfd);
				noninvasiveness = 2;
			}
		} else {
			P->ptrace_count = 1;
			if (Ppush_state(P, PS_RUN) == NULL)
				goto bad;
			P->ptraced = TRUE;
			P->ptrace_halted = TRUE;
		}
	}

	/*
	 * Noninvasive grab, or error doing possibly-noninvasive grab promoted
	 * it to definite noninvasiveness.
	 */
	if (*perr || noninvasiveness > 1) {
		dt_dprintf("%i: grabbing noninvasively.\n", P->pid);
		P->noninvasive = TRUE;
	}

	snprintf(procname, sizeof (procname), "%s/%d/exe",
	    procfs_path, P->pid);

	if ((Pread_isa_info(P, procname)) < 0)
		/* error already reported */
		goto bad_untrace;

	_dprintf ("Pgrab: grabbed PID %i.\n", P->pid);

	return P;
bad_untrace:
	Puntrace(P, 0);
bad:
	*perr = errno;
	Pfree_internal(P);
	return NULL;
}

/*
 * Free a process control structure.
 *
 * Close the file descriptors but don't do the Prelease logic, nor replace
 * breakpoints with their original content.  (Thus, extremely internal.)
 */
static void
Pfree_internal(struct ps_prochandle *P)
{
	Pclose(P);
	Psym_free(P);

	free(P->auxv);
	free(P->bkpts);
	free(P);
}

/*
 * Free a process control structure, external version.

 * Aborts if the process is not already released.
 */
void
Pfree(struct ps_prochandle *P)
{
	if (P == NULL)
		return;

	assert(P->released);

	Pfree_internal(P);
}

/*
 * Close the process's cached file descriptors and expensive state.
 *
 * It is reloaded when needed.
 */
void
Pclose(struct ps_prochandle *P)
{
	if (!P)
		return;

	Preset_maps(P);
	if (P->memfd > -1) {
		close(P->memfd);
		P->memfd = -1;
	}
	if (P->mapfilefd > -1) {
		close(P->mapfilefd);
		P->mapfilefd = -1;
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
	/*
	 * Act like free() with respect to null pointers: a null pointer is dead,
	 * by definition.
	 */
	if (P == NULL)
		return PS_DEAD;

	return (P->state);
}

/*
 * Return the open memory file descriptor for the process, reopening it if
 * needed.
 *
 * Although technically unnecessary, also prohibit digging about in process
 * memory if the process is not being ptrace()d, since such digging is risky if
 * we cannot robustly stop the process before it hits regions of interest.
 *
 * Clients must not close this file descriptor, nor use it after the process is
 * freed or Pclose()d.
 */
int
Pmemfd(struct ps_prochandle *P)
{
	char procname[PATH_MAX];
	char *fname;

	if ((P->memfd != -1) || P->noninvasive)
		return (P->memfd);

	snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)P->pid);
	fname = procname + strlen(procname);

	strcpy(fname, "mem");
	if ((P->memfd = open(procname, O_RDONLY | O_EXCL | O_CLOEXEC)) < 0) {
		_dprintf("Pmemfd: failed to open %s: %s\n",
		    procname, strerror(errno));
		return (-1);
	}

	return (P->memfd);
}

/*
 * Return an fd to the /proc/<pid>/map_files directory, suitable for openat().
 *
 * Returns -1 on any failure at all: the caller must compensate.
 *
 * -ENOENT failures are 'sticky' and will cause this function to fail
 * persistently, throughout the run.
 */
int
Pmapfilefd(struct ps_prochandle *P)
{
	char procname[PATH_MAX];
	char *fname;
	static int no_map_files;

	if (no_map_files)
		return (-1);

	if (P->mapfilefd != -1)
		return (P->mapfilefd);

	snprintf(procname, sizeof (procname), "%s/%d/",
	    procfs_path, (int)P->pid);
	fname = procname + strlen(procname);

	strcpy(fname, "map_files");
	P->mapfilefd = open(procname, O_PATH | O_CLOEXEC | O_DIRECTORY);

	/*
	 * Nonexistent-file failures are permanent: this is a kernel that does
	 * not support map_files at all, so repeatedly reopening is a waste of
	 * time.
	 */
	if ((P->mapfilefd < 0) && (errno == ENOENT))
		no_map_files = 1;

	return (P->mapfilefd);
}

/*
 * Return 1 if the process is dynamically linked.
 */
int
Pdynamically_linked(struct ps_prochandle *P)
{
	/*
	 * This is populated by the r_debug computation code, so call it now,
	 * and fail if it fails.
	 */
	if (r_debug(P) < 0)
		return -1;

	return !P->no_dyn;
}

/*
 * Release the process.
 *
 * release_mode is one of the PS_RELEASE_* constants:
 *
 * PS_RELEASE_NORMAL: detach, remove breakpoints, but do not kill
 *
 * PS_RELEASE_KILL: kill the process and detach.
 *
 * PS_RELEASE_NO_DETACH: do not resume, unpoke breakpoints, or detach.  This
 * lets the caller reinitialize libproc state without affecting a process halted
 * at exec().  We also do not call the lock hook in this situation, balancing a
 * lack of lock hook invocation in Pgrab() when already_ptraced is set: this
 * means that other threads cannot sneak in in the brief instant while the
 * ps_prochandle no longer exists.
 *
 * Note: unlike Puntrace(), this operation (in PS_RELEASE_NORMAL mode) releases
 * *all* outstanding traces, cleans up all breakpoints, and detaches
 * unconditionally.  It's meant to be used if the tracer is dying or completely
 * losing interest in its tracee, not merely if it doesn't want to trace it
 * right now.
 *
 * This function is safe to call repeatedly.
 */
void
Prelease(struct ps_prochandle *P, int release_mode)
{
	if (!P)
		return;

	if (P->released)
		return;

	Psym_release(P);

	while (P->ptrace_states.dl_next != NULL)
		Ppop_state(P);

	if (P->state == PS_DEAD) {
		_dprintf("Prelease: releasing handle %p of dead pid %d\n",
		    (void *)P, (int)P->pid);

		bkpt_flush(P, 0, TRUE);
		P->released = TRUE;

		dt_debug_dump(0);

		return;
	}

	bkpt_flush(P, 0, release_mode == PS_RELEASE_NO_DETACH);

	_dprintf("Prelease: releasing handle %p pid %d\n",
	    (void *)P, (int)P->pid);

	if (release_mode == PS_RELEASE_KILL)
		kill(P->pid, SIGKILL);
	else if (P->ptraced && (release_mode != PS_RELEASE_NO_DETACH))
		wrapped_ptrace(P, PTRACE_DETACH, (int)P->pid, 0, 0);

	if (P->ptrace_count != 0 && ptrace_lock_hook &&
	    release_mode != PS_RELEASE_NO_DETACH)
		ptrace_lock_hook(P, P->wrap_arg, 0);

	P->state = PS_DEAD;
	P->released = TRUE;

	dt_debug_dump(0);
}

/*
 * Wait for the process to stop for any reason, possibly blocking.
 *
 * (A blocking wait will be automatically followed by as many nonblocking waits
 * as are necessary to drain the queue of requests and leave the child in a
 * state capable of handling more ptrace() requests -- or dead.)
 *
 * Returns the number of state changes processed, or -1 on error.
 *
 * The debugging strings starting "process status change" are relied upon by the
 * libproc/tst.signals.sh test.
 */
long
Pwait_internal(struct ps_prochandle *P, boolean_t block)
{
	long err;
	long num_waits = 0;
	long one_wait;
	int status;

	/*
	 * If we wait for something to happen while stopped at a breakpoint, we
	 * will deadlock, especially if what we get is another SIGTRAP.  So
	 * demote the Pwait() to a nonblocking one, in case a significant state
	 * change (e.g. death) has happened that we need to know about.
	 */
	if (P->bkpt_halted)
		block = 0;

	/*
	 * If we are waiting for a pending stop, but pending_stops is zero, we
	 * know an earlier Pwait() (whether from a breakpoint handler or from
	 * another thread) has raced with this one and consumed the stop.  A
	 * blocking Pwait() will follow: in order not to block forever, we must
	 * convert it to nonblocking.
	 */
	if (P->awaiting_pending_stops && P->pending_stops == 0 && block)
		block = 0;

	/*
	 * Never do a blocking wait for a process we already know is dead or
	 * trace-stopped.  (A process that is merely stopped can be waited for:
	 * we will still receive state transitions for it courtesy of
	 * PTRACE_LISTEN.)
	 */
	if (P->state == PS_TRACESTOP)
		block = 0;

	/*
	 * Never wait at all for a process we already know is dead: the PID may
	 * have been reallocated, and we'll end up waiting for the wrong
	 * process.
	 */
	if (P->state == PS_DEAD)
		return (0);

	do
	{
		errno = 0;
		err = waitpid(P->pid, &status, __WALL | (!block ? WNOHANG : 0));

		switch (err) {
		case 0:
			return(0);
		case -1:
			if (errno == ECHILD) {
				P->state = PS_DEAD;
				return (0);
			}

			if (errno != EINTR) {
				_dprintf("Pwait: error waiting: %s\n",
				    strerror(errno));
				return (-1);
			}
		}
	} while (errno == EINTR);

	if (Pwait_handle_waitpid(P, status) < 0)
		return (-1);

	/*
	 * Now repeatedly loop, processing more waits until none remain.
	 */
	do {
		one_wait = Pwait(P, 0);
		num_waits += one_wait;
	} while (one_wait > 0);

	return (num_waits + 1);
}

/*
 * Change the process status according to the status word returned by waitpid().
 *
 * Returns -1 on error.
 */
static int
Pwait_handle_waitpid(struct ps_prochandle *P, int status)
{
	uintptr_t ip = 0;

	if (WIFCONTINUED(status)) {
		_dprintf("%i: process got SIGCONT.\n", P->pid);
		P->state = PS_RUN;
		return (0);
	}

	/*
	 * Exit under ptrace() monitoring.  We have to detect, and ignore,
	 * PTRACE_EVENT_EXIT, because we need PTRACE_EVENT_EXIT to reliably
	 * detect the early vanishing of TRACEFORKed children, for which we must
	 * have PTRACE_EVENT_EXIT turned on before the fork() happens.	However,
	 * we cannot mark the process dead at this stage, because the kernel
	 * will not fire proc::exit until later: if we consider the process dead
	 * at this stage, and dtrace terminates as a result, it will miss that
	 * firing.
	 */
	if ((status >> 8) == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
		_dprintf("%i: process status change: exit coming.\n", P->pid);

		wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL, 0);
		return (0);
	}

	/*
	 * Now the real exit detection.
	 */
	if (WIFEXITED(status)) {
		_dprintf("%i: process status change: exited with "
		    "exitcode %i\n", P->pid, WEXITSTATUS(status));

		P->state = PS_DEAD;
		return (0);
	}

	/*
	 * PTRACE_INTERRUPT stop or group-stop.
	 */
	if ((status >> 16) == PTRACE_EVENT_STOP) {

		/*
		 * This is a PTRACE_INTERRUPT trickling in, or a group-stop
		 * trickling in *after* resumption but before SIGCONT arrives.
		 * Check pending_stops and group_stopped to see if we are
		 * expecting an interrupt, or have changed state in such a way
		 * that a stop is no longer expected: if so, just resume.
		 */

		if (WSTOPSIG(status) == SIGTRAP) {
			P->listening = 0;
			if (P->group_stopped) {
				/*
				 * We change the state to PS_RUN here to
				 * immediately indicate to callers of Pstate()
				 * that the process has been resumed: the
				 * SIGCONT's arrival also sets it, but this
				 * generally has no effect.
				 */
				_dprintf("%i: group-stop ending, SIGCONT expected soon.\n",
				    P->pid);
				wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL, 0);
				P->group_stopped = 0;
				P->state = PS_RUN;
			} else if (P->pending_stops > 0) {
				/*
				 * Expected PTRACE_INTERRUPT.  Do not resume.
				 */
				_dprintf("%i: process status change: "
				    "PTRACE_INTERRUPTed.\n", P->pid);

				P->pending_stops--;
				P->state = PS_TRACESTOP;
			} else {
				/*
				 * Unexpected / latent PTRACE_INTERRUPT.  Resume
				 * automatically.
				 */
				wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL, 0);
				_dprintf("%i: Unexpected PTRACE_EVENT_STOP, "
				    "resuming automatically.\n", P->pid);
				P->state = PS_RUN;
			}
		} else if ((P->listening) && (P->pending_stops > 0)) {
			/*
			 * PTRACE_INTERRUPT during PTRACE_LISTEN.  Do not
		         * resume: transition out of listening state, but not
		         * out of group-stop.
		         */
			_dprintf("%i: process status change: no longer "
			    "LISTENing, PTRACE_INTERRUPTed.\n", P->pid);

			P->listening = 0;
			P->pending_stops--;
			P->state = PS_TRACESTOP;
		} else if ((P->state == PS_RUN) || (P->state == PS_STOP)) {
			/*
			 * Group-stop: we will already be in stopped state, if
			 * need be.  LISTEN for further changes, but do not
			 * resume.  (If already traced, Puntrace() will do a
			 * LISTEN for us.)
			 */
			_dprintf("%i: Process status change: group-stop: "
			    "LISTENed.\n", P->pid);
			P->group_stopped = 1;
			P->listening = 1;
			P->state = PS_STOP;

			wrapped_ptrace(P, PTRACE_LISTEN, P->pid, NULL,
			    NULL);
		} else {
			_dprintf("%i: random PTRACE_EVENT_STOP.\n", P->pid);
		}

		return (0);
	}

	/*
	 * Hit by a signal after PTRACE_LISTEN.  Resend it and resume (causing a
	 * signal-delivery stop, to be processed on the next cycle).
	 */

	if (WIFSTOPPED(status) &&
	    (WSTOPSIG(status) != SIGTRAP) &&
	    ((status >> 16) == PTRACE_EVENT_STOP)) {
		_dprintf("%i: process status change: child got stopping signal, "
		    "received after PTRACE_LISTEN: reinjecting.\n",
		    WSTOPSIG(status));
		wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL,
		    WSTOPSIG(status));
		return(0);
	}

	/*
	 * Signal-delivery-stop.  Possibly adjust process state, and reinject.
	 */
	if ((WIFSTOPPED(status)) && (WSTOPSIG(status) != SIGTRAP)) {

		switch (WSTOPSIG(status)) {
		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			_dprintf("%i: process status change: child got "
			    "stopping signal %i.\n", P->pid, WSTOPSIG(status));
			P->state = PS_STOP;
			break;
		case SIGCONT:
			_dprintf("%i: process status change: SIGCONT.\n",
			    P->pid);
			P->state = PS_RUN;
			break;
		default: ;
			_dprintf("%i: process status change: child got "
			    "signal %i.\n", P->pid, WSTOPSIG(status));
			/*
			 * Not a stopping signal.
			 */
		}
		if (P->ptraced) {
			wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL,
			    WSTOPSIG(status));
			P->state = PS_RUN;
		}
		return(0);
	}

	/*
	 * A terminating signal. If this is not a SIGTRAP, reinject it.
	 * (SIGTRAP is handled further down.)
	 */
	if ((WIFSIGNALED(status)) && (WTERMSIG(status) != SIGTRAP)) {
		_dprintf("%i: process status change: child got "
		    "terminating signal %i.\n", P->pid, WTERMSIG(status));

		wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL, WTERMSIG(status));
		return(0);
	}

	/*
	 * TRACEEXEC trap.  Our breakpoints are gone, our auxv has changed, our
	 * memfd needs closing for a later reopen, we need to figure out if this
	 * is a 64-bit process anew, any (internal) exec() handler should be
	 * called, and we need to seize control again.
	 *
	 * Note: much of this may be unnecessary if the caller has defined an
	 * exec() pad to longjmp() to, but there's no guarantee that the caller
	 * will always Pfree() us after that jump, so do it anyway.
	 *
	 * We are always tracing the thread group leader, so most of the
	 * complexity around execve() tracing goes away.
	 *
	 * We can't do much about errors here: if we lose control, we lose
	 * control.
	 */
	if ((status >> 8) == (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {

		char procname[PATH_MAX];
		jmp_buf *exec_jmp;

		_dprintf("%i: process status change: exec() detected, "
		    "resetting...\n", P->pid);

		P->state = PS_TRACESTOP;
		Pclose(P);

		snprintf(procname, sizeof (procname), "%s/%d/exe",
		    procfs_path, P->pid);

		/*
		 * This reports its error itself, but if there *is* an
		 * error, there's not much we can do.  Leave elf64 et al
		 * unchanged: most of the time, the ELF class and
		 * bitness don't change across exec().
		 */
		Pread_isa_info(P, procname);

		bkpt_flush(P, 0, TRUE);

		free(P->auxv);
		P->auxv = NULL;
		P->nauxv = 0;
		P->tracing_bkpt = 0;
		P->bkpt_halted = 0;
		P->bkpt_consume = 0;
		P->r_debug_addr = 0;
		P->info_valid = 0;
		P->group_stopped = 0;
		P->listening = 0;
		if ((P->ptrace_count == 0) && ptrace_lock_hook)
			ptrace_lock_hook(P, P->wrap_arg, 1);

		while (P->ptrace_states.dl_next != NULL)
			Ppop_state(P);
		P->ptrace_count = 1;
		Ppush_state(P, PS_RUN);
		P->ptraced = TRUE;
		P->ptrace_halted = TRUE;
		P->state = PS_TRACESTOP;

		/*
		 * If we have an exec jump buffer, jump to it.  Do not move out
		 * of trace-stop: the caller can do that after resetting itself
		 * properly.
		 */
		exec_jmp = *(libproc_unwinder_pad(P));
		if (exec_jmp != NULL)
			longjmp(*exec_jmp, 1);

		Puntrace(P, 0);

		return(0);
	}

	/*
	 * TRACEFORK or TRACEVFORK trap.  Remove the breakpoints in the new
	 * child, since we don't trace forked children (yet).  All other state
	 * remains unchanged.
	 */
	if (((status >> 8) == (SIGTRAP | PTRACE_EVENT_FORK << 8)) ||
	    ((status >> 8) == (SIGTRAP | PTRACE_EVENT_VFORK << 8)))
	{
		unsigned long pid;		/* Cannot be pid_t */

		P->state = PS_TRACESTOP;
		if (wrapped_ptrace(P, PTRACE_GETEVENTMSG, P->pid, NULL, &pid) < 0)
			_dprintf("%i: process status change: fork() or vfork() "
			    "detected but PID cannot be determined: %s; ignoring.\n",
			    P->pid, strerror(errno));
		else {
			_dprintf("%lu: process status change: fork() or vfork() "
			    "detected, discarding breakpoints...\n", pid);

			/*
			 * Flush breakpoints in the child and detach from it,
			 * once it has halted.
			 */
			ignored_child_wait(P, pid, ignored_child_flush);
		}
		if (wrapped_ptrace(P, PTRACE_CONT, P->pid, 0, 0) < 0)
			_dprintf("%i: cannot continue parent: %s\n", P->pid,
			    strerror(errno));
		else
			_dprintf("%i: continued parent.\n", P->pid);
		P->state = PS_RUN;
		return(0);
	}

	/*
	 * TRACECLONE trap.  A new thread (or something near enough to it that
	 * we can treat it as one).  DTrace is not ready for this everywhere
	 * yet: stop monitoring shared library activity, if we were.
	 */
	if ((status >> 8) == (SIGTRAP | PTRACE_EVENT_CLONE << 8))
	{
		unsigned long pid;		/* Cannot be pid_t */

		_dprintf("%i: process status change: thread creation detected, "
			    "suppressing rd events...\n", P->pid);

		P->state = PS_TRACESTOP;
		if (wrapped_ptrace(P, PTRACE_GETEVENTMSG, P->pid, NULL,
			&pid) < 0)
			ignored_child_wait(P, pid, ignored_child_flush);

		rd_event_suppress(P->rap);

		/*
		 * Stop tracing thread creation now.
		 */
		wrapped_ptrace(P, PTRACE_SETOPTIONS, 0, LIBPROC_PTRACE_OPTIONS);

		P->state = PS_RUN;
		wrapped_ptrace(P, PTRACE_CONT, pid, 0, 0);
		return(0);
	}

        /*
	 * Other ptrace() traps are generally unexpected unless some breakpoints
	 * are active.  We can only get this far when ptrace()ing, so we know it
	 * must be valid to call ptrace() ourselves.
	 *
	 * Trap, possibly breakpoint trap.
	 *
	 * If we are in the midst of processing a breakpoint on a machine with
	 * hardware singlestepping, we already know our breakpoint address.
	 * Otherwise -- and always, in the case of software singlestepping -- we
	 * must acquire the tracee's IP address and verify that it really is a
	 * breakpoint and not a random third-party SIGTRAP, and pass it on if
	 * not.
	 *
	 * If bkpt_consume is turned on, we want to simply consume the trap
	 * without invoking the breakpoint handler.  (This is used when doing a
	 * Pbkpt_continue() on a singlestepping SIGSTOPPed process which may or
	 * may not already have trapped.)
	 */

	P->state = PS_TRACESTOP;

#ifndef NEED_SOFTWARE_SINGLESTEP
	ip = P->tracing_bkpt;
#endif

	if (ip == 0) {
		bkpt_t *bkpt;
		ip = Pget_bkpt_ip(P, 0);
		bkpt = bkpt_by_addr(P, ip, FALSE);

		if (((unsigned long) ip == -1) || !bkpt) {
			int sig = 0;
			/*
			 * This is not a known breakpoint nor a temporary
			 * singlestepping breakpoint.  Reinject it.
			 */
			_dprintf("Pwait: %i: process status change at "
			    "address %lx: signal %i/%i does not correspond to "
			    "a known breakpoint.\n", P->pid, ip,
			    WTERMSIG(status), WSTOPSIG(status));

			if ((WIFSTOPPED(status)) && (WSTOPSIG(status)) == SIGTRAP)
				P->state = PS_STOP;

			if (((WIFSIGNALED(status)) && (WTERMSIG(status) == SIGTRAP)) ||
			    ((WIFSTOPPED(status)) && (WSTOPSIG(status)) == SIGTRAP))
				sig = SIGTRAP;

			wrapped_ptrace(P, PTRACE_CONT, P->pid, NULL, sig);
			return(0);
		}
	}

	if (!P->bkpt_consume)
		P->state = bkpt_handle(P, ip);
	return(0);
}

/*
 * Wait for a random traced child that we do not otherwise care about to halt,
 * then detach from it after calling 'fun'.
 *
 * This child is not one we are doing anything in particular to, other than
 * calling 'fun': we expect only PTRACE_EVENT_STOP / EXEC / CLONE / FORK /
 * VFORK, termination, or signals.  We pass on all the signals, return on
 * termination, and detach when a STOP is seen.  When a CLONE or VFORK are seen,
 * recursively call ourselves to do the same thing to the new child.  If an EXEC
 * is seen, note that fun() should not be called, since its purpose is to fix up
 * the new child for future detached operation by doing things like removing
 * breakpoints, and on exec() all such things are gone.
 *
 * Errors are ignored, because there's not much we can do about them.
 */
static void
ignored_child_wait(struct ps_prochandle *P, pid_t pid,
    void (*fun)(struct ps_prochandle *P, pid_t pid))
{
	int status;

	_dprintf("%i: waiting for ignored child %i to halt\n", P->pid, pid);

	while (1) {
		do {
			errno = 0;
			if (waitpid(pid, &status, __WALL | __WNOTHREAD) < 0)
				if ((errno != -EINTR) && (errno != 0))
					return;
		} while (errno == -EINTR);

		/*
		 * Stop. Call the fun() (if set), detach, and return.
		 */
		if ((status >> 16) == PTRACE_EVENT_STOP) {
			if (fun)
				fun(P, pid);
			if (wrapped_ptrace(P, PTRACE_DETACH, pid, 0, 0) < 0)
				_dprintf("Cannot detach from ignored %i\n", pid);

			return;
		}

		/*
		 * Process terminated.
		 */
		if ((status >> 8) == (SIGTRAP | PTRACE_EVENT_EXIT << 8)) {
			if (wrapped_ptrace(P, PTRACE_DETACH, pid, 0, 0) < 0)
				_dprintf("Cannot resume dying ignored %i\n",
				    pid);

			return;
		}

		/*
		 * Signal delivery stops.  Reinject them.
		 */
		if (WIFSTOPPED(status)) {
			wrapped_ptrace(P, PTRACE_CONT, pid, NULL,
			    WSTOPSIG(status));
		}

		if (WIFSIGNALED(status)) {
			wrapped_ptrace(P, PTRACE_CONT, pid, NULL,
			    WTERMSIG(status));
		}

		/*
		 * fork() or vfork() trap.  Just get going again (via a
		 * recursive call).
		 */
		if (((status >> 8) == (SIGTRAP | PTRACE_EVENT_FORK << 8)) ||
		    ((status >> 8) == (SIGTRAP | PTRACE_EVENT_VFORK << 8))) {
			unsigned long new_pid;	/* Cannot be pid_t */
			if (wrapped_ptrace(P, PTRACE_GETEVENTMSG, pid, NULL,
				&new_pid) == 0) {
				_dprintf("%i: recursive ignored fork()/clone().\n",
				    pid);
				ignored_child_wait(P, new_pid, fun);
			}
		}

		/*
		 * exec(). We don't want to call the fun() any more.
		 */
		if ((status >> 8) == (SIGTRAP | PTRACE_EVENT_EXEC << 8))
			fun = NULL;
	}
}

/*
 * Called to dispose of breakpoints in a fork()ed child, once it is traceable.
 */
static void
ignored_child_flush(struct ps_prochandle *P, pid_t pid)
{
	bkpt_flush(P, pid, FALSE);
}

/*
 * If true, detach the process when no ptraces or breakpoints are outstanding.
 * Otherwise, leave it attached, but running.
 */
void
Ptrace_set_detached(struct ps_prochandle *P, boolean_t detach)
{
	P->detach = detach;
}

/*
 * Push a state onto the state stack.  Return that state, in case the caller
 * wants to mutate it.
 */
static prev_states_t *
Ppush_state(struct ps_prochandle *P, int state)
{
	prev_states_t *s;

	s = malloc(sizeof (struct prev_states));
	if (s == NULL)
		return NULL;

	s->state = state;
	dt_list_prepend(&P->ptrace_states, s);
	_dprintf("Ppush_state(): ptrace_count %i, state %i\n", P->ptrace_count, s->state);

	return s;
}

/*
 * Pop a state off the state stack and return it.
 */
static int
Ppop_state(struct ps_prochandle *P)
{
	prev_states_t *s;
	int state;

	s = dt_list_next(&P->ptrace_states);
	dt_list_delete(&P->ptrace_states, s);
	_dprintf("Ppop_state(): ptrace_count %i, state %i\n", P->ptrace_count+1, s->state);
	state = s->state;
	free(s);
	return state;
}

/*
 * Change the state at the top of the stack.
 */
static void
Pset_orig_state(struct ps_prochandle *P, int state)
{
	prev_states_t *s;

	s = dt_list_next(&P->ptrace_states);
	if (s)
		s->state = state;
}

/*
 * Grab a ptrace(), unless one is already grabbed: increment the ptrace count.
 * Regardless, the process will be PS_TRACESTOP on return if 'stopped' (unless
 * the process is dead or un-ptraceable).
 *
 * Returns zero or a positive number on success, or a negative error code.
 *
 * Failure to trace is considered an error unless called from within Pgrab().
 */
int
Ptrace(struct ps_prochandle *P, int stopped)
{
	int err = 0;
	prev_states_t *state;

	if (P->noninvasive)
		return -ESRCH;

	if (P->ptrace_count == 0 && ptrace_lock_hook)
		ptrace_lock_hook(P, P->wrap_arg, 1);

	P->ptrace_count++;
	state = Ppush_state(P, P->state);
	if (state == NULL) {
		err = -ENOMEM;
		goto err2;
	}

	if (P->ptraced) {
		int listen_interrupt;

		/*
		 * In this case, we must take care that anything already queued
		 * for Pwait() is correctly processed, before demanding a stop.
		 * We should also not try to stop something that is already
		 * stopped in ptrace(), nor try to resume it.  We know the
		 * PTRACE_INTERRUPT has arrived when the traceee is no longer in
		 * run state.
		 *
		 * Additionally, if the tracee is already stopped by a SIGSTOP
		 * and we are LISTENing for it, waiting is expected: we want to
		 * hang on until the PTRACE_INTERRUPT is processed, since only
		 * that event clears the listening state and makes it possible
		 * for other ptrace requests to succeed.
		 */
		Pwait(P, 0);
		state->state = P->state;
		if ((!stopped) || (P->state == PS_TRACESTOP))
			return(0);

		if (P->state == PS_DEAD)
			return PS_DEAD;

		listen_interrupt = P->listening;
		P->ptrace_halted = TRUE;

		wrapped_ptrace(P, PTRACE_INTERRUPT, P->pid, 0);

		P->pending_stops++;
		P->awaiting_pending_stops++;
		while (P->pending_stops &&
		    ((P->state == PS_RUN) ||
			(listen_interrupt && P->listening)))
			Pwait(P, 1);
		P->awaiting_pending_stops--;

		return 0;
	}

	if (wrapped_ptrace(P, PTRACE_SEIZE, P->pid, 0, LIBPROC_PTRACE_OPTIONS |
		    PTRACE_O_TRACECLONE) < 0) {
		if (!Pgrabbing)
			goto err;
		else
			goto err2;
	}

	P->ptraced = TRUE;

	if (stopped) {
		P->ptrace_halted = TRUE;

		if (wrapped_ptrace(P, PTRACE_INTERRUPT, P->pid, 0, 0) < 0) {
			wrapped_ptrace(P, PTRACE_DETACH, P->pid, 0, 0);
			if (!Pgrabbing)
				goto err;
			else
				goto err2;
		}

		/*
		 * Now wait for the interrupt to trickle in.
		 */
		P->pending_stops++;
		P->awaiting_pending_stops++;
		while (P->pending_stops && P->state == PS_RUN) {
			if (Pwait(P, 1) == -1)
				goto err;
		}
		P->awaiting_pending_stops--;

		if ((P->state != PS_TRACESTOP) &&
		    (P->state != PS_STOP)) {
			err = -ECHILD;
			goto err2;
		}
	}

	return err;
err:
	err = errno;
err2:
	P->ptrace_count--;

	if (P->ptrace_count == 0 && ptrace_lock_hook)
		ptrace_lock_hook(P, P->wrap_arg, 0);

	if (err != -ECHILD)
		return err;
	else
		return -ESRCH; /* for a clearer message */
}

/*
 * Ungrab a ptrace(), setting the process running if the balancing Ptrace() call
 * stopped it. If none are left, set running, or detach if P->detach.
 * If stuck at a breakpoint, continue from the breakpoint, poking text back in.
 *
 * If 'leave_stopped' is set, the process is not restarted upon Puntrace(), even
 * if the balancing Ptrace() stopped it: it is up to the caller to resume.
 *
 * Lots of P->state comparisons are needed here, because a PTRACE_CONT is only
 * valid if the process was stopped beforehand: otherwise, you get -ESRCH,
 * which is helpfully indistinguishable from the error you get when the process
 * is dead.
 */
void
Puntrace(struct ps_prochandle *P, int leave_stopped)
{
	int prev_state;

	/*
	 * Protect against unbalanced Ptrace()/Puntrace().
	 */
	if ((!P->ptraced) || (P->ptrace_count == 0))
		return;

	P->ptrace_count--;
	prev_state = Ppop_state(P);

	/*
	 * Still under Ptrace(), or a stay in stop state requested, or stuck in
	 * stop state by request of an earlier breakpoint handler?  OK, nothing
	 * needs doing, except possibly a resume, using Pbkpt_continue() so as
	 * to do the right thing if a breakpoint is outstanding.
	 */

	if (P->ptrace_count || P->bkpt_halted || leave_stopped ||
	    prev_state != PS_RUN) {
		if ((prev_state == PS_RUN) && (P->state == PS_TRACESTOP)) {
			_dprintf("%i: Continuing because previous state was "
			    "PS_RUN\n", P->pid);
			/*
			 * Pbkpt_continue() will usually reset our previous state,
			 * but it leaves it unchanged if it finds that we were not
			 * stopped at a breakpoint.  Do it by hand in that case.
			 */
			if (!Pbkpt_continue(P))
				P->state = prev_state;
			P->ptrace_halted = FALSE;
		} else if ((prev_state == PS_STOP) &&
		    (P->state == PS_TRACESTOP)) {
			P->state = prev_state;
			P->group_stopped = 1;
			P->listening = 1;
			_dprintf("%i: LISTENing because previous state was "
			    "PS_STOP\n", P->pid);
			wrapped_ptrace(P, PTRACE_LISTEN, P->pid, NULL,
			    NULL);
		}

		if (P->ptrace_count == 0 && ptrace_lock_hook)
			ptrace_lock_hook(P, P->wrap_arg, 0);

		return;
	}

	/*
	 * Not in Ptrace(): at top level, not halted at a breakpoint.
	 *
	 * Continue the process, or detach it if requested, no breakpoints
	 * are outstanding, and no rd_agent is active.
	 */
	if ((!P->detach) || P->rap || (P->num_bkpts > 0)) {
		if (P->state == PS_TRACESTOP) {
			_dprintf("%i: Continuing.\n", P->pid);
			if (!Pbkpt_continue(P))
				P->state = PS_RUN;
			P->ptrace_halted = FALSE;
			Pwait(P, 0);
		}
	} else {
		_dprintf("%i: Detaching.\n", P->pid);
		P->state = PS_RUN;
		P->ptraced = FALSE;
		if ((wrapped_ptrace(P, PTRACE_DETACH, P->pid, 0, 0) < 0) &&
		    (errno == ESRCH))
			P->state = PS_DEAD;
		P->ptrace_halted = FALSE;
		P->info_valid = 0;
	}

	if (P->ptrace_count == 0 && ptrace_lock_hook)
		ptrace_lock_hook(P, P->wrap_arg, 0);
}

/*
 * Given a breakpoint address, find the corresponding bkpt structure, or NULL if
 * none.  If delete is set, remove it from the hash too.
 */
static bkpt_t
*bkpt_by_addr(struct ps_prochandle *P, uintptr_t addr,
	int delete)
{
	uint_t h = addr % BKPT_HASH_BUCKETS;
	bkpt_t *last_bkpt = NULL;
	bkpt_t *bkpt;

	for (bkpt = P->bkpts[h]; bkpt != NULL; bkpt = bkpt->bkpt_next) {
		if (bkpt->bkpt_addr == addr) {
			if (delete) {
				if (last_bkpt)
					last_bkpt->bkpt_next = bkpt->bkpt_next;
				else
					P->bkpts[h] = bkpt->bkpt_next;
				bkpt->bkpt_next = NULL;
			}
			return (bkpt);
		}
		last_bkpt = bkpt;
	}

	return NULL;
}

/*
 * Return a given machine word with the breakpoint instruction masked over the
 * start of it.
 */
static unsigned long
mask_bkpt(unsigned long word)
{
	union {
		unsigned long insn;
		char bkpt[sizeof (unsigned long)];
	} bkpt;

	bkpt.insn = word;
	memcpy(bkpt.bkpt, (char *) plat_bkpt, sizeof (plat_bkpt));
	return bkpt.insn;
}

/*
 * Introduce a breakpoint on a particular address with the given handler.
 *
 * The breakpoint handler should return nonzero to remain stopped at this4
 * breakpoint, or zero to continue.  The cleanup handler can clean up any
 * additional state associated with this breakpoint's 'data' on Punbkpt() or
 * Prelease().
 *
 * If after_singlestep, the handler is called after singlestepping, rather than
 * before. In both cases, the process is trace-stopped when the call happens,
 * and the instruction at the breakpoint site is the original instruction
 * (not the breakpoint instruction).
 *
 * The handler returns a PS_ value that indicates what it wants doing to
 * the process:
 *  PS_RUN: continue past breakpoint
 *  PS_TRACESTOP, PS_STOP: stop at breakpoint
 *  PS_DEAD: process was killed
 *
 * Calling this function on an existing breakpoint cleans up the old one's state
 * and reassigns it.
 *
 * Returns 0 on success, or an errno value on failure.
 */
int
Pbkpt(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    int (*bkpt_handler) (uintptr_t addr, void *data),
    void (*bkpt_cleanup) (void *data),
    void *data)
{
	if (P->noninvasive)
		return -ESRCH;

	return add_bkpt(P, addr, after_singlestep, FALSE, bkpt_handler,
	    bkpt_cleanup, data);
}

/*
 * A notifier is just like a breakpoint, except that it cannot change the
 * control flow of the program.  One address can have only one breakpoint
 * associated with it, but as many notifiers as you like.  A notifier shares
 * the same after-singlestepping property as any breakpoint at its address, but
 * has its own cleanup and private data.
 */
int
Pbkpt_notifier(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    void (*bkpt_handler) (uintptr_t addr, void *data),
    void (*bkpt_cleanup) (void *data),
    void *data)
{
	if (P->noninvasive)
		return -ESRCH;

	return add_bkpt(P, addr, after_singlestep, TRUE,
	    (int (*) (uintptr_t addr, void *data)) bkpt_handler,
	    bkpt_cleanup, data);
}

static int
add_bkpt(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    int is_notifier, int (*bkpt_handler) (uintptr_t addr, void *data),
    void (*bkpt_cleanup) (void *data),
    void *data)
{
	bkpt_t *bkpt = bkpt_by_addr(P, addr, FALSE);
	uint_t h = addr % BKPT_HASH_BUCKETS;
	bkpt_handler_t *notifier = NULL;
	int err;

	/*
	 * Already present? Just tweak it.
	 */
	if (bkpt && !is_notifier) {
		if (bkpt->bkpt_handler.bkpt_cleanup)
			bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);
		bkpt->bkpt_handler.bkpt_handler = bkpt_handler;
		bkpt->bkpt_handler.bkpt_cleanup = bkpt_cleanup;
		bkpt->bkpt_handler.bkpt_data = data;
		bkpt->after_singlestep = after_singlestep;
		return 0;
	}

	/*
	 * Prepare the notifier structure.  Adding a notifier multiple times
	 * leads to its being called multiple times.
	 */
	if (is_notifier) {
		notifier = malloc(sizeof (struct bkpt_handler));
		if (!notifier)
			return -ENOMEM;
		memset(notifier, 0, sizeof (struct bkpt_handler));

		notifier->bkpt_handler = bkpt_handler;
		notifier->bkpt_cleanup = bkpt_cleanup;
		notifier->bkpt_data = data;

		if (bkpt) {
			dt_list_append(&bkpt->bkpt_notifiers, notifier);
			return 0;
		}
	}

	err = Ptrace(P, 1);
	if (err < 0)
		goto err2;

	/*
	 * Allocate and poke in a new breakpoint.  This breakpoint may have no
	 * handler, if this is a notifier addition.
	 */

	bkpt = malloc(sizeof (struct bkpt));
	if (!bkpt)
		goto err;

	memset(bkpt, 0, sizeof (struct bkpt));
	if (!is_notifier) {
		bkpt->bkpt_handler.bkpt_handler = bkpt_handler;
		bkpt->bkpt_handler.bkpt_cleanup = bkpt_cleanup;
		bkpt->bkpt_handler.bkpt_data = data;
	} else
		dt_list_append(&bkpt->bkpt_notifiers, notifier);

	bkpt->after_singlestep = after_singlestep;
	bkpt->bkpt_addr = addr;

	errno = 0;
	bkpt->orig_insn = wrapped_ptrace(P, PTRACE_PEEKTEXT, P->pid,
	    addr, 0);
	if (errno != 0) {
		free(bkpt);
		goto err;
	}
	if (wrapped_ptrace(P, PTRACE_POKETEXT, P->pid, addr,
		mask_bkpt(bkpt->orig_insn)) < 0) {
		free(bkpt);
		goto err;
	}

	bkpt->bkpt_next = P->bkpts[h];
	P->bkpts[h] = bkpt;
	P->num_bkpts++;

	_dprintf("%i: Added breakpoint on %lx\n", P->pid, addr);

	/*
	 * Breakpoint added.  Because at least one breakpoint is in force, this
	 * Puntrace() will have the effect of resuming the child iff it is the
	 * topmost such, but never of detaching, no matter what the state of
	 * P->detach.
	 */
	Puntrace(P, 0);
	return 0;
err:
	err = errno;
err2:
	_dprintf("%i: Cannot add breakpoint on %lx: %s\n", P->pid,
	    addr, strerror(errno));
	Puntrace(P, 0);
	free(notifier);
	return err;
}

/*
 * Remove a breakpoint on a given address, if one exists there, or (if we are
 * currently in a handler for that address) arrange to do it later.
 *
 * The cleanup handlers, if any, are called, in reverse order of handler
 * addition.
 */
void
Punbkpt(struct ps_prochandle *P, uintptr_t addr)
{
	bkpt_t *bkpt;
	int err;
	int orig_ptrace_halted = P->ptrace_halted;

	/*
	 * Sanity check, twice, to avoid bugs leading to underflow.
	 *
	 * Halt the child and Pwait() before we start manipulating the
	 * breakpoint hash, to avoid incoming traps on breakpoints we have
	 * stopped tracking.
	 */

	if (P->num_bkpts == 0) {
		_dprintf("%i: Punbkpt() called with %lx, but no breakpoints are "
		    "outstanding.\n", P->pid, addr);
		return;
	}

	err = Ptrace(P, 1);
	if (err < 0) {
		_dprintf("%i: Unexpected error %s ptrace()ing to remove "
		    "breakpoint.", P->pid, strerror(err));
		return;
	}

	/*
	 * If we are currently inside a handler for this breakpoint, or inside
	 * the temporary singlestepping breakpoint associated with it, arrange
	 * to remove it later, after handler execution is complete.  Only do a
	 * lookup-with-unchain if this is not so.
	 */
	bkpt = bkpt_by_addr(P, addr, FALSE);
	if (!bkpt) {
		_dprintf("%i: Punbkpt() called with %lx, which is not a known "
		    "breakpoint.\n", P->pid, addr);
		Puntrace(P, 0);
		return;
	}

	if (bkpt->in_handler) {
		bkpt->pending_removal = 1;
		Puntrace(P, 0);
		return;
	}

	if (bkpt->bkpt_singlestep && bkpt->bkpt_singlestep->in_handler) {
		bkpt->bkpt_singlestep->pending_removal = 1;
		Puntrace(P, 0);
		return;
	}

	Pwait(P, 0);
	bkpt = bkpt_by_addr(P, addr, TRUE);

	P->num_bkpts--;

	/*
	 * If we are not singlestepping past this breakpoint now, we have to
	 * poke the old content back in (and we delegate to Punbkpt_child_poke()
	 * to do that).  Otherwise, we just have to adjust our instruction
	 * pointer and resume (and the resumption is done automatically for us
	 * when we Puntrace() in any case).
	 */

	if (P->tracing_bkpt != bkpt->bkpt_addr)
		Punbkpt_child_poke(P, 0, bkpt);
	else {
		_dprintf("%i: Breakpoint at %lx already poked back, changing "
		    "instruction pointer\n", P->pid, bkpt->bkpt_addr);
		if (Preset_bkpt_ip(P, P->tracing_bkpt) < 0)
			switch (errno) {
			case ESRCH:
				_dprintf("%i: -ESRCH, process is dead.\n",
				    P->pid);
				P->state = PS_DEAD;
				return;
			case 0: break;
			default:
				_dprintf("%i: Unknown error doing instruction "
				    "pointer adjustment while removing "
				    "breakpoint: %s\n", P->pid,
				    strerror(errno));
			}

		P->tracing_bkpt = 0;
		P->bkpt_halted = 0;

		/*
		 * If we were stopped by this breakpoint rather than by a nested
		 * Ptrace(), set ourselves running again on Puntrace().
		 */
		if (!orig_ptrace_halted)
			Pset_orig_state(P, PS_RUN);
	}

	delete_bkpt_handler(bkpt);

	Puntrace(P, 0);

	_dprintf("%i: Removed breakpoint on %lx\n", P->pid, addr);
}

/*
 * Poke back the pre-breakpoint text into the given breakpoint, in the process
 * tracked by prochandle P, or (if pid is nonzero) into that process instead.
 * (In the latter case, no process state changes happen on error, either.)
 */
static void
Punbkpt_child_poke(struct ps_prochandle *P, pid_t pid, bkpt_t *bkpt)
{
	uintptr_t insn;
	pid_t child_pid = pid;

	if (child_pid == 0)
		child_pid = P->pid;

	/*
	 * Only overwrite the breakpoint insn if it is still a breakpoint.  If
	 * it has changed (perhaps due to a new text section being mapped in),
	 * do nothing.
	 */
	errno = 0;
	insn = wrapped_ptrace(P, PTRACE_PEEKTEXT, child_pid,
	    bkpt->bkpt_addr, 0);

	if (errno == 0 && insn == mask_bkpt(insn) &&
	    wrapped_ptrace(P, PTRACE_POKETEXT, child_pid,
		bkpt->bkpt_addr, bkpt->orig_insn) < 0)
		switch (errno) {
		case ESRCH:
			_dprintf("%i: -ESRCH, process is dead.\n", child_pid);
			if (!pid)
				P->state = PS_DEAD;
			return;
		case EIO:
		case EFAULT:
			/* The address in the child has disappeared. */
			_dprintf("%i: Instruction pokeback into %lx failed: "
			    "%s\n", child_pid, bkpt->bkpt_addr,
			    strerror(errno));
		case 0: break;
		default:
			_dprintf("%i: Unknown error removing breakpoint:"
			    "%s\n", child_pid, strerror(errno));
		}
}

/*
 * Discard breakpoint state.
 *
 * Done when the child process is dead or being released or its address space
 * has vanished due to an exec(), or when the child has (v)fork()ed.
 *
 * Rendered more complicated by the need to do local breakpoint cleanup even if
 * the process is gone, and by the fact that in fork() or clone() situations we
 * want to clean up remote breakpoints but not the local ones.  (We cannot use
 * Punbkpt() in the former situation because the process might have exec()ed,
 * and we do not want to continue it, nor modify the address of breakpoints that
 * can no longer exist within it: we also need to cancel out the 'in local
 * handler' flag which otherwise stops breakpoints being removed immediately,
 * since if this function is called any active handlers will never be
 * re-entered: either the ps_prochandle is about to disappear, or the process
 * has just exec()ed, thus is running, thus we cannot be inside a breakpoint
 * handler, since that implies the process is stopped.)
 *
 * If the pid is nonzero, the local breakpoint state is not cleaned up: if
 * 'gone' is nonzero, the child's breakpoints are not cleaned up.  (If both are
 * nonzero, we do not define what happens, but it's not useful.)
 */
static void
bkpt_flush(struct ps_prochandle *P, pid_t pid, int gone) {
	size_t i;

	_dprintf("Flushing breakpoints.\n");

	/*
	 * Consume all SIGTRAPs from here until flush completion.
	 */
	if (!pid)
		P->bkpt_consume = 1;

	for (i = 0; i < BKPT_HASH_BUCKETS; i++) {
		bkpt_t *bkpt;
		bkpt_t *old_bkpt = NULL;

		for (bkpt = P->bkpts[i]; bkpt != NULL;
		     old_bkpt = bkpt, bkpt = bkpt->bkpt_next) {
			if (old_bkpt != NULL) {
				old_bkpt->in_handler = FALSE;
				if (pid)
					Punbkpt_child_poke(P, pid, old_bkpt);
				else if (!gone)
					Punbkpt(P, old_bkpt->bkpt_addr);
				else {
					bkpt_t *bkpt = bkpt_by_addr(P,
					    old_bkpt->bkpt_addr, TRUE);
					delete_bkpt_handler(bkpt);
				}
			}
		}

		if (old_bkpt != NULL) {
			if (pid)
				Punbkpt_child_poke(P, pid, old_bkpt);
			else if (!gone) {
				old_bkpt->in_handler = FALSE;
				Punbkpt(P, old_bkpt->bkpt_addr);
			} else {
				bkpt_t *bkpt = bkpt_by_addr(P, old_bkpt->bkpt_addr, TRUE);
				delete_bkpt_handler(bkpt);
			}
		}
	}

	/*
	 * Only do this state-varying stuff if we're not chewing over an alien
	 * PID.
	 */
	if (!pid) {
		/*
		 * One last Pwait() to consume a potential trap on the last now-dead
		 * breakpoint.
		 */
		if (!gone)
			Pwait(P, 0);

		P->bkpt_consume = 0;
		P->tracing_bkpt = 0;
		P->bkpt_halted = 0;
		P->num_bkpts = 0;
	}
}

/*
 * Delete a single breakpoint handler's state, calling cleanups as needed.
 *
 * Frees the breakpoint, but does not unhash it: the caller must do that
 * beforehand.
 */
static void
delete_bkpt_handler(struct bkpt *bkpt)
{
	bkpt_handler_t *deleting, *next;
	deleting = dt_list_prev(&bkpt->bkpt_notifiers);

	if (deleting) {
		next = dt_list_prev(deleting);
		do {
			if (deleting->bkpt_cleanup)
				deleting->bkpt_cleanup(deleting->bkpt_data);

			dt_list_delete(&bkpt->bkpt_notifiers, deleting);
			free(deleting);
			deleting = next;
		} while (deleting != NULL);
	}

	if (bkpt->bkpt_handler.bkpt_cleanup)
		bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);

	free(bkpt);
}

/*
 * Handle a breakpoint, upon receipt of a SIGTRAP.
 *
 * Returns a process state.
 */
static int
bkpt_handle(struct ps_prochandle *P, uintptr_t addr)
{
	/*
	 * We have either have stopped at an address we know, or we are
	 * singlestepping past one of our breakpoints and have ended a hardware
	 * singlestep or hit the temporary singlestepping breakpoint; our caller
	 * has already validated that this breakpoint is known.
	 *
	 * But first, decree that we are already tracestopped, for the sake of
	 * any handlers that rely on this.
	 */

	P->state = PS_TRACESTOP;

	if (P->tracing_bkpt == addr)
		return bkpt_handle_post_singlestep(P,
		    bkpt_by_addr(P, P->tracing_bkpt, FALSE));
	else {
		bkpt_t *bkpt = bkpt_by_addr(P, addr, FALSE);

		if (bkpt->bkpt_singlestep) {
			bkpt_t *real_bkpt = bkpt->bkpt_singlestep;
			Punbkpt(P, bkpt->bkpt_addr);
			return bkpt_handle_post_singlestep(P, real_bkpt);
		}

		if (P->tracing_bkpt != 0) {
			_dprintf("%i: Nested breakpoint detected, probable bug.\n",
			    P->pid);
			/*
			 * Nested breakpoint.  Oo-er.  Probably an explicit
			 * continue by the caller.  Overwrite the original addr
			 * with a breakpoint, if known.  (Do not call the
			 * handler: we are long past the breakpoint address at
			 * this point.  Do not even trap errors.  If you do
			 * this, you are on your own!)
			 */
			bkpt_t *bkpt = bkpt_by_addr(P, P->tracing_bkpt, FALSE);
			if (bkpt) {
				uintptr_t orig_insn;
				orig_insn = wrapped_ptrace(P, PTRACE_PEEKTEXT,
				    P->pid, bkpt->bkpt_addr, 0);
				wrapped_ptrace(P, PTRACE_POKETEXT, P->pid,
				    bkpt->bkpt_addr, mask_bkpt(orig_insn));

				/*
				 * If the 'original' instruction is a breakpoint,
				 * the caller has overwritten the breakpoint
				 * itself: do not remember it.
				 */
				if (orig_insn != mask_bkpt(orig_insn))
					bkpt->orig_insn = orig_insn;
			}
			P->tracing_bkpt = 0;
		}

		return bkpt_handle_start(P, bkpt);
	}
}

/*
 * Handle a breakpoint at the breakpoint insn itself.  Overwrite the breakpoint
 * instruction, possibly call the handler, and run.
 *
 * Returns a process state.
 */
static int
bkpt_handle_start(struct ps_prochandle *P, bkpt_t *bkpt)
{
	if (wrapped_ptrace(P, PTRACE_POKETEXT, P->pid,
		bkpt->bkpt_addr, bkpt->orig_insn) < 0)
		switch (errno) {
		case ESRCH:
			return PS_DEAD;
		case 0:
			_dprintf("%i: Hit %lx, setting insn to %lx\n",
			    P->pid, bkpt->bkpt_addr, bkpt->orig_insn);
			break;
		case EIO:
		case EFAULT:
			/*
			 * The address in the child has disappeared.  This
			 * cannot happen: we just stopped there!
			 */
		default:
			_dprintf("Unexpected error removing breakpoint on PID %i:"
			    "%s\n", P->pid, strerror(errno));
			return PS_TRACESTOP;
		}

	P->tracing_bkpt = bkpt->bkpt_addr;

	if (!bkpt->after_singlestep) {
		int state = PS_RUN;
		bkpt_handler_t *notifier = dt_list_next(&bkpt->bkpt_notifiers);

		bkpt->in_handler++;
		P->bkpt_halted = 1;
		for (; notifier; notifier = dt_list_next(notifier)) {
			void (*notify) (uintptr_t, void *) =
			    (void (*) (uintptr_t, void *)) notifier->bkpt_handler;

			notify(bkpt->bkpt_addr, notifier->bkpt_data);
		}

		if (bkpt->bkpt_handler.bkpt_handler) {
			state = bkpt->bkpt_handler.bkpt_handler(bkpt->bkpt_addr,
			    bkpt->bkpt_handler.bkpt_data);

			/*
			 * PS_STOP always means that the process was stopped by
			 * SIGSTOP: we remain able to listen for state changes
			 * on it (e.g. when PTRACE_INTERRUPTed).  A process
			 * halted by a breakpoint is already interrupted and
			 * will never change state without our connivance: so
			 * translate PS_STOPs from breakpoint handlers into
			 * PS_TRACESTOP.
			 */
			if (state == PS_STOP)
				state = PS_TRACESTOP;
			_dprintf("%i: Breakpoint handler returned %i\n", P->pid,
			    state);
		}
		bkpt->in_handler--;

		if (state != PS_RUN)
			return state;

		P->bkpt_halted = 0;

		/*
		 * If the handler wants to erase the breakpoint, do so. Punbkpt() will
		 * automatically continue for us.
		 */
		if (bkpt->pending_removal) {
			Punbkpt(P, bkpt->bkpt_addr);
			return PS_RUN;
		}
	}

	return Pbkpt_continue_internal(P, bkpt, TRUE);
}

/*
 * Continue a process, possibly stopped at a breakpoint.
 *
 * Return zero if it left the process state unchanged.
 */
int
Pbkpt_continue(struct ps_prochandle *P)
{
	bkpt_t *bkpt = bkpt_by_addr(P, P->tracing_bkpt, FALSE);
	uintptr_t ip;

	if (!P->ptraced)
		return 0;

	/*
	 * We don't know where we are.  We might be stopped at an erased
	 * breakpoint; we might not be stopped at a breakpoint at all: we might
	 * not even be ptracing the process (or it might have done something
	 * that prohibits our tracing it, like exec()ed a setuid process).  Just
	 * issue a continue.
	 */
	if (P->tracing_bkpt == 0 || !bkpt) {
		if (wrapped_ptrace(P, PTRACE_CONT, P->pid, 0, 0) < 0) {
			int err = errno;
			if (err == ESRCH) {
				if ((kill(P->pid, 0) < 0) && errno == ESRCH)
					P->state = PS_DEAD;
			}
			/*
			 * Since we must have an outstanding Ptrace() anyway,
			 * assume that an EPERM means that this process is not
			 * stopped: it cannot mean that we aren't allowed to
			 * ptrace() it.
			 */
			if (err != EPERM) {
				_dprintf("%i: Unexpected error resuming: %s\n",
				    P->pid, strerror(err));
				return 1;
			}
		}
		P->state = PS_RUN;
		return 1;
	}

	/*
	 * We are at a breakpoint.  We could be stopped on the breakpoint locus,
	 * or elsewhere, depending on whether we have singlestepped already and
	 * on whether we were hit by a SIGSTOP while that happened: or we might
	 * not be stopped at all.
	 *
	 * We can only determine this by comparing the current IP address with
	 * the breakpoint address.
	 */

	ip = Pget_bkpt_ip(P, 1);
	if (ip == 0) {
		/*
		 * Not stopped at all.  Just do a quick Pwait().
		 */
		Pwait(P, 0);
		return 0;
	} else if (ip == P->tracing_bkpt)
		/*
		 * Still need to singlestep.
		 */
		P->state = Pbkpt_continue_internal(P, bkpt, TRUE);
	else {
		/*
		 * No need to singlestep, but may need to consume
		 * a SIGTRAP.
		 */
		P->bkpt_consume = 1;
		Pwait(P, 0);
		P->bkpt_consume = 0;
		P->state = Pbkpt_continue_internal(P, bkpt, FALSE);
	}

	return 1;
}

/*
 * Continue a process stopped at a given breakpoint locus, resetting the
 * instruction pointer and issuing a singlestep if needed, or calling
 * bkpt_handle_post_singlestep() to call the breakpoint handler, poke the
 * breakpoint instruction back in, and resume execution.
 */
static int
Pbkpt_continue_internal(struct ps_prochandle *P, bkpt_t *bkpt, int singlestep)
{
	P->bkpt_halted = 0;

	if (singlestep) {
		if (Preset_bkpt_ip(P, bkpt->bkpt_addr) != 0)
			goto err;

		/*
		 * If hardware singlestepping is supported, this is easy.
		 * Otherwise, drop a temporary breakpoint associated with this
		 * one.
		 */
#ifndef NEED_SOFTWARE_SINGLESTEP
		if (wrapped_ptrace(P, PTRACE_SINGLESTEP, P->pid, 0, 0) != 0)
			goto err;
#else
		uintptr_t next_ip = Pget_next_ip(P);
		bkpt_t *next_bkpt = bkpt_by_addr(P, next_ip, FALSE);

		/*
		 * Only drop a temporary breakpoint if there isn't already a
		 * breakpoint there.  (If there is one, various things will go
		 * somewhat wrong: in particular, it's impossible to call any
		 * after_singlestep handlers on the original breakpoint.  DTrace
		 * never does this, so we should be fine.)
		 */
		if (next_bkpt == NULL) {
			Pbkpt(P, next_ip, FALSE, NULL, NULL, NULL);
			next_bkpt = bkpt_by_addr(P, next_ip, FALSE);
			next_bkpt->bkpt_singlestep = bkpt;
		}

		/*
		 * If the next breakpoint is the same as the current one, emit a
		 * warning and delete the breakpoint: we cannot implement this
		 * without instruction emulation, since we cannot drop a
		 * breakpoint on this instruction and execute it at the same
		 * time.
		 */
		if (next_bkpt == bkpt) {
			fprintf(stderr, "%i; Breakpoint loops are unimplemented on "
			    "this platform: breakpoint at %lx deleted.\n", P->pid,
			    bkpt->bkpt_addr);
			Punbkpt(P, bkpt->bkpt_addr);
		}

		if (wrapped_ptrace(P, PTRACE_CONT, P->pid, 0, 0) != 0)
			goto err;
#endif

		return PS_RUN;

	err:
		if (errno == ESRCH)
			return PS_DEAD;
		else
			return PS_TRACESTOP;
	}

	return bkpt_handle_post_singlestep(P, bkpt);
}

/*
 * Do everything necessary after singlestepping past a breakpoint.  When called,
 * we are known to have completed a singlestep over the specified breakpoint and
 * to be in trace-stop state.
 *
 * Returns a process state.
 */
static int
bkpt_handle_post_singlestep(struct ps_prochandle *P, bkpt_t *bkpt)
{
	int state = PS_RUN;
	unsigned long orig_insn;

	if (bkpt->after_singlestep) {
		bkpt_handler_t *notifier = dt_list_next(&bkpt->bkpt_notifiers);

		bkpt->in_handler++;
		for (; notifier; notifier = dt_list_next(notifier)) {
			void (*notify) (uintptr_t, void *) =
			    (void (*) (uintptr_t, void *)) notifier->bkpt_handler;

			notify(bkpt->bkpt_addr, notifier->bkpt_data);
		}

		if (bkpt->bkpt_handler.bkpt_handler) {
			state = bkpt->bkpt_handler.bkpt_handler(bkpt->bkpt_addr,
			    bkpt->bkpt_handler.bkpt_data);
			/*
			 * See comment in bkpt_handle_start().
			 */
			if (state == PS_STOP)
				state = PS_TRACESTOP;
			_dprintf("%i: Breakpoint handler returned %i\n", P->pid,
			    state);
		}
		bkpt->in_handler--;
	}

	/*
	 * If the handler wants to erase the breakpoint, do so. Punbkpt() will
	 * automatically continue for us.
	 */
	if (bkpt->pending_removal) {
		Punbkpt(P, bkpt->bkpt_addr);
		return PS_RUN;
	}

	/*
	 * If the handler wants us to stay single-stepping, obey it.  (We assume
	 * that if it sets the state to PS_DEAD, it has killed the process
	 * itself.)
	 */
	if (state != PS_RUN) {
		P->bkpt_halted = 1;
		return state;
	}

	/*
	 * The handler wants us to run. Resume.
	 *
	 * Always re-peek the original instruction, in case of self-modifying
	 * code.  If it fails, hold on to the old one.
	 */
	errno = 0;
	orig_insn = wrapped_ptrace(P, PTRACE_PEEKTEXT, P->pid, bkpt->bkpt_addr, 0);
	if (errno != 0) {
		_dprintf("Unexpected error re-peeking original instruction "
		    "at %lx on PID %i: %s\n", bkpt->bkpt_addr, P->pid,
		    strerror(errno));
	}
	else
		bkpt->orig_insn = orig_insn;

	/*
	 * The error handling here is verbose and annoying, but unavoidable, as
	 * the process could be SIGKILLed at any time, even between these
	 * ptrace() calls.
	 */
	if (wrapped_ptrace(P, PTRACE_POKETEXT, P->pid, bkpt->bkpt_addr,
		mask_bkpt(orig_insn)) < 0)
		switch (errno) {
		case ESRCH:
			return PS_DEAD;
		case 0:
			break;
		case EIO:
		case EFAULT:
			_dprintf("%i: Post-singlestep at %lx but %s: unbkptting\n",
			    P->pid, bkpt->bkpt_addr, strerror(errno));
			/*
			 * The address in the child has disappeared.  Probably a
			 * very unlucky unmap after singlestepping across pages.
			 * Delete the breakpoint.
			 */
			Punbkpt(P, bkpt->bkpt_addr);
			break;
		default:
			_dprintf("Unexpected error reinserting breakpoint on "
			    "PID %i: %s\n", P->pid, strerror(errno));
			return PS_TRACESTOP;
		}

	/*
	 * We are no longer stopped at a breakpoint.
	 */
	P->tracing_bkpt = 0;

	if (wrapped_ptrace(P, PTRACE_CONT, P->pid, 0, 0) < 0) {
		switch (errno) {
		case ESRCH:
			return PS_DEAD;
		case 0: break;
		case EIO:
		case EFAULT:
		default:
			_dprintf("Strange error continuing after breakpoint "
			    "on PID %i: %s\n", P->pid, strerror(errno));
			return PS_TRACESTOP;
		}
	}

	return PS_RUN;
}

/*
 * If we are stopped at a breakpoint, return its address, or 0 if none.
 */
uintptr_t Pbkpt_addr(struct ps_prochandle *P)
{
	return P->tracing_bkpt;
}

ssize_t
Pread(struct ps_prochandle *P,
	void *buf,		/* caller's buffer */
	size_t nbyte,		/* number of bytes to read */
	uintptr_t address)	/* address in process */
{
	if (address < LONG_MAX)
		return (pread(Pmemfd(P), buf, nbyte, (loff_t)address));
	else {
		/*
		 * High-address copying.
		 *
		 * pread() won't work here because the offset is larger than a
		 * valid file offset, so we have to use PTRACE_PEEKDATA.  This
		 * is exceptionally painful and inefficient.
		 */

		int state;
		uintptr_t saddr = (address & ~((uintptr_t) sizeof (long) - 1));
		size_t i, sz;
		long *rbuf;

		size_t len = nbyte + (address - saddr);
		if (len % sizeof (long) != 0)
			len += sizeof (long) - (len % sizeof (long));

		rbuf = malloc(len);
		if (!rbuf)
			return 0;

		/*
		 * We have to read into an intermediate buffer because alignment
		 * constraints forbid us from dumping data into the
		 * buf in unaligned fashion.
		 */

		state = Ptrace(P, 1);
		if (state < 0) {
			free(rbuf);
			return 0;
		}

		errno = 0;
		for (i = 0, sz = 0; sz < len; i++, sz += sizeof (long)) {
			long data = wrapped_ptrace(P, PTRACE_PEEKDATA, P->pid,
			    saddr + sz, NULL);
			if (errno != 0)
				break;
			rbuf[i] = data;
		}

		nbyte = (sz > nbyte) ? nbyte : sz;

		memcpy(buf, rbuf + (address - saddr), nbyte);
		free(rbuf);
		Puntrace(P, state);

		return nbyte;
	}
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

/*
 * Read from process, doing endianness and size conversions, possibly quashing
 * address-related errors.
 */
ssize_t
Pread_scalar_quietly(struct ps_prochandle *P,
    void *buf,			/* caller's buffer */
    size_t nbyte,		/* number of bytes to read */
    size_t nscalar,		/* scalar size on this platform */
    uintptr_t address,		/* address in process */
    int quietly)		/* do not emit error messages */
{
	size_t copybytes;

	union
	{
		uint8_t b1;
		uint16_t b2;
		uint32_t b4;
		uint64_t b8;
	} conv;

	conv.b8 = 0;

	if (nbyte > sizeof (conv.b8)) {
		_dprintf("Pread_scalar(): Attempt to read scalar of size %lu "
		    "greater than max supported size %lu from PID %i\n",
		    nbyte, sizeof (conv.b8), P->pid);
		return (-1);
	}

	if (nscalar > sizeof (conv.b8)) {
		_dprintf("Pread_scalar(): Attempt to read into scalar of size %lu "
		    "greater than max supported size %lu from PID %i\n",
		    nscalar, sizeof (conv.b8), P->pid);
		return (-1);
	}

	/*
	 * Prohibit 32-on-64 debugging.  Just too much chance of data loss.
	 * DTrace is 64-bit anyway so only an autobuilder error could cause this.
	 */
	if (nbyte > nscalar) {
		_dprintf("Pread_scalar(): Attempt to read scalar of size %lu into "
		    "scalar of size %lu PID %i: narrowing conversions are not "
		    "supported\n", nbyte, nscalar, P->pid);
		return (-1);
	}

	if (Pread(P, (void *)&conv.b1, nbyte, address) != nbyte) {
		if (!quietly)
			_dprintf("Pread_scalar(): attempt to read %lu bytes from PID %i at "
			    "address %lx read fewer bytes than expected.\n", nbyte,
			    P->pid, address);
		return(-1);
	}

	memset(buf, 0, nscalar);
#if __BYTE_ORDER == __BIG_ENDIAN
	buf = (char *) buf + nscalar - nbyte;
	copybytes = nbyte;
#else
	copybytes = nscalar;
#endif
	switch (nscalar) {
	case sizeof(conv.b1): memcpy(buf, &conv.b1, copybytes); break;
	case sizeof(conv.b2): memcpy(buf, &conv.b2, copybytes); break;
	case sizeof(conv.b4): memcpy(buf, &conv.b4, copybytes); break;
	case sizeof(conv.b8): memcpy(buf, &conv.b8, copybytes); break;
	default: _dprintf("Pread_scalar(): Attempt to read into a scalar of "
	    "%lu bytes is not supported.\n", nscalar);
		return(-1);
	}

	return nbyte;
}

/*
 * Read from process, doing endianness and size conversions, with noisy errors
 * if things go wrong.
 */
ssize_t
Pread_scalar(struct ps_prochandle *P,
    void *buf,			/* caller's buffer */
    size_t nbyte,		/* number of bytes to read */
    size_t nscalar,		/* scalar size on this platform */
    uintptr_t address)		/* address in process */
{
	return Pread_scalar_quietly(P, buf, nbyte, nscalar, address, 0);
}

pid_t
Pgetpid(struct ps_prochandle *P)
{
	if (P == NULL)
		return -1;
	return (P->pid);
}

/*
 * Set the path to /proc.
 */
void
Pset_procfs_path(const char *path)
{
	strcpy(procfs_path, path);
}

/*
 * Set a function returning a pointer to a jmp_buf we should use to throw
 * exceptions.
 */
void
Pset_libproc_unwinder_pad(libproc_unwinder_pad_fun *unwinder_pad)
{
	libproc_unwinder_pad = unwinder_pad;
}

/*
 * Set the ptrace() lock hook.
 */
void
Pset_ptrace_lock_hook(ptrace_lock_hook_fun *hook)
{
	ptrace_lock_hook = hook;
}

/*
 * Return 1 if the process is invasively grabbed, and thus ptrace()able.
 */
int
Ptraceable(struct ps_prochandle *P)
{
	return !P->noninvasive;
}

/*
 * Return 1 if this is a 64-bit process.
 */
int
Pelf64(struct ps_prochandle *P)
{
	return P->elf64;
}

/*
 * Return 1 if this process exists (or, in the presence of PID wraparound, some
 * other process that happens to have the same PID).
 *
 * WARNING: intrinsically racy: use only when this is not a problem!
 *
 * Note: This takes a pid, not a ps_prochandle, because it is used to decide
 * how to grab a process, before a ps_prochandle exists.
 */
int
Pexists(pid_t pid)
{
	char procname[PATH_MAX];

	snprintf(procname, sizeof (procname), "%s/%d",
	    procfs_path, pid);
	return access(procname, F_OK) == 0;
}

/*
 * Return 1 if this process has a controlling terminal.
 *
 * Note: This takes a pid, not a ps_prochandle, because it is used to decide
 * how to grab a process, before a ps_prochandle exists.
 */
static int
Phastty(pid_t pid)
{
	char procname[PATH_MAX];
	char *buf = NULL;
	char *s;
	size_t n;
	FILE *fp;
	int tty;

	snprintf(procname, sizeof (procname), "%s/%d/stat",
	    procfs_path, pid);

	if ((fp = fopen(procname, "r")) == NULL) {
		_dprintf("Phastty: failed to open %s: %s\n",
		    procname, strerror(errno));
		return -1;
	}

	if (getline(&buf, &n, fp) < 0) {
		free(buf);
		fclose(fp);
		return -1;
	}

	fclose(fp);
	s = strchr (buf, ')');
	if (!s) {
		free(buf);
		return -1;
	}

	s++;

	if (sscanf(s, " %*c %*d %*d %*d %d", &tty) != 1) {
		free(buf);
		return -1;
	}

	_dprintf("%i: tty: %i\n", pid, tty);

	free(buf);
	return (tty != 0);
}

/*
 * Get a specific field out of /proc/$pid/status and return the portion after
 * the colon in a new dynamically allocated string, or NULL if no field matches.
 */
char *
Pget_proc_status(pid_t pid, const char *field)
{
	char status[PATH_MAX];
	FILE *fp;
	char *line = NULL;
	size_t len;

	snprintf(status, sizeof(status), "/proc/%i/status", pid);

	if ((fp = fopen(status, "r")) == NULL) {
		_dprintf("Process is dead.\n");
		return NULL;
	}

	while (getline(&line, &len, fp) >= 0) {
		if ((strncmp(line, field, strlen(field)) == 0) &&
		    (strncmp(line + strlen(field), ":\t", 2) == 0)) {

			memmove(line, line + strlen(field) + 2,
			    strlen(line + strlen(field) + 2) + 1);
			_dprintf("%li: %s: %s", (long) pid, status, line);
			fclose(fp);
			return line;
		}
	}
	free(line);
	fclose(fp);
	return NULL;
}

/*
 * Return the (real) uid of this process.
 *
 * Note: This takes a pid, not a ps_prochandle, because it is used to decide
 * how to grab a process, before a ps_prochandle exists.
 */
static uid_t
Puid(pid_t pid)
{
	char *uids = Pget_proc_status(pid, "Uid");
	uid_t uid;

	if (!uids) {
		_dprintf("Cannot get uids for PID %i: assuming zero.\n", pid);
		return 0;
	}

	uid = strtol(uids, NULL, 10);
	_dprintf("%i: uid is %i.\n", pid, uid);
	free(uids);

	return uid;
}

/*
 * Return 1 if this process is a system daemon, 0 if it is not, and -1 on error.
 *
 *  A process is considered a system daemon if either of two possibilities hold:
 *
 *   - a systemd cgroup exists, and the process is in the sysslice or the root
 *     slice
 *
 *   - if no systemd cgroup exists, the process is in the system UID range (less
 *     than 'useruid'), it has no controlling terminal and the standard streams
 *     are not pointing to TTYs or regular files outside /var.  (i.e. standard
 *     streams pointing to things like /dev/null, pipes, or sockets are not
 *     determinative: only TTYs and regular files are.)
 */
int
Psystem_daemon(pid_t pid, uid_t useruid, const char *sysslice)
{
	FILE *fp;
	char procname[PATH_MAX];
	char *buf = NULL;
	size_t n;
	int fd;

	/*
	 * If this is a system running systemd, or we don't know yet, dig out
	 * the systemd cgroup line from /proc/$pid/cgroup.
	 */
	if (systemd_system != 0) {
		snprintf(procname, sizeof (procname), "%s/%d/cgroup",
		    procfs_path, pid);

		if ((fp = fopen(procname, "r")) == NULL) {
			_dprintf("Psystem_daemon: failed to open %s: %s\n",
			    procname, strerror(errno));
			return -1;
		}

		while (getline(&buf, &n, fp) >= 0) {
			if (strstr(buf, ":name=systemd:") != NULL) {
				systemd_system = 1;
				break;
			}
		}
		fclose(fp);
		if (systemd_system < 0)
			systemd_system = 0;
	}

	/*
	 * We have the systemd cgroup line in buf.  Look at our slice name.
	 */
	if (systemd_system) {
		char *colon = strchr(buf, ':');
		if (colon)
			colon = strchr(colon + 1, ':');

		_dprintf("systemd system: sysslice: %s; colon: %s\n", sysslice, colon);
		if (colon &&
		    (strncmp(colon, sysslice, strlen(sysslice)) == 0)) {
			free(buf);
			_dprintf("%i is a system daemon process.\n", pid);
			return 1;
		}
		free(buf);
		return 0;
	}
	free(buf);

	/*
	 * This is not a systemd system -- we have to guess by looking at the
	 * process's UID, controlling terminal, and the TTYness and/or location
	 * of the files pointed to by its stdin/out/err.  (i.e. we first
	 * consider whether something may be a system daemon by consulting its
	 * uid range and controlling TTY, then try to rule it out by looking for
	 * open fds to TTYs and regular files outside particular subtrees.)  (As
	 * a consequence of these rules, a process with no standard streams at
	 * all is considered a system daemon -- this is a cheap way of catching
	 * kernel threads.)
	 */
	if ((Puid(pid) > useruid) || Phastty(pid))
		return 0;

	for (fd = 0; fd < 3; fd++) {
		struct stat s;
		char buf[6];

		snprintf(procname, sizeof (procname), "%s/%d/fd/%d",
		    procfs_path, pid, fd);

		/*
		 * Can't be a regular file or a TTY if we can't stat it: on to
		 * the next fd.
		 */
		if (stat(procname, &s) < 0)
			continue;

		/*
		 * See if this is a regular file pointing outside /var.
		 * readlink() truncate the path.
		 */
		memset(buf, '\0', sizeof(buf));
		if (S_ISREG(s.st_mode) &&
		    (readlink(procname, buf, sizeof(buf)-1) == 0)) {
			if((buf[0] == '/') && (strcmp(buf, "/var/") != 0))
				return 0;
		}

		/*
		 * See if this fd is a TTY: if it is, this isn't a system daemon.
		 */

		if (S_ISCHR(s.st_mode)) {
			int fdf;
			int tty = 0;

			fdf = open(procname, O_RDONLY | O_CLOEXEC | O_NOCTTY |
			    O_NOATIME | O_NONBLOCK);

			if (fdf >= 0) {
				tty = isatty(fdf);
				close(fdf);
			}

			if (tty)
				return 0;
		}
	}

	/*
	 * In system UID range, no controlling TTY, no FDs that refer to TTYs at
	 * all, no FDs that refer to regular files outside /var.  This is a
	 * system daemon.
	 */
	_dprintf("%i is a system daemon process.\n", pid);
	return 1;
}
