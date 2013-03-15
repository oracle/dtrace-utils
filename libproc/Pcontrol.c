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
 * Copyright 2010 -- 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
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

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <mutex.h>
#include <platform.h>

#include "Pcontrol.h"
#include "libproc.h"

int	_libproc_debug;		/* set non-zero to enable debugging printfs */

char	procfs_path[PATH_MAX] = "/proc";

static	int Pgrabbing = 0; 	/* A Pgrab() is underway. */

static void Pfree(struct ps_prochandle *P);
static int bkpt_handle(struct ps_prochandle *P, uintptr_t addr);
static int bkpt_handle_start(struct ps_prochandle *P, bkpt_t *bkpt);
static int bkpt_handle_singlestep(struct ps_prochandle *P, bkpt_t *bkpt);
static int Pbkpt_continue_internal(struct ps_prochandle *P, bkpt_t *bkpt);
static void bkpt_flush(struct ps_prochandle *P, int gone);

/*
 * This is the library's .init handler.
 */
_dt_constructor_(_libproc_init)
static void
_libproc_init(void)
{
	_libproc_debug = getenv("DTRACE_DEBUG") != NULL;
}

/*
 * If _libproc_debug is set, printf the debug message to stderr
 * with an appropriate prefix.
 */
_dt_printflike_(1,2)
void
_dprintf(const char *format, ...)
{
       if (_libproc_debug) {
               va_list alist;

               va_start(alist, format);
               fputs("libproc DEBUG: ", stderr);
               vfprintf(stderr, format, alist);
               va_end(alist);
       }
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
	size_t len)	/* size of the path buffer */
{
	struct ps_prochandle *P;
	char procname[PATH_MAX];
	pid_t pid;
	int rc;
	int status;

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		*perr = ENOMEM;
		return (NULL);
	}

	memset(P, 0, sizeof (*P));
	P->bkpts = calloc(BKPT_HASH_BUCKETS, sizeof (struct bkpt_t *));
	if (!P->bkpts) {
		fprintf(stderr, "Out of memory initializing breakpoint hash\n");
		exit(1);
	}

	if ((pid = fork()) == -1) {
		free(P);
		*perr = EAGAIN;
		return (NULL);
	}

	if (pid == 0) {			/* child process */
		id_t id;

		ptrace(PTRACE_TRACEME, 0, 0, 0);

		/*
		 * If running setuid or setgid, reset credentials to normal,
		 * then wait for our parent to set up exec options.
		 */
		if ((id = getgid()) != getegid())
			(void) setgid(id);
		if ((id = getuid()) != geteuid())
			(void) setuid(id);

		raise(SIGSTOP);
		(void) execvp(file, argv);  /* execute the program */
		_exit(127);
	}

	/*
	 * Initialize the process structure.
	 */
	P->state = PS_TRACESTOP;
	P->ptrace_count++;
	P->ptraced = TRUE;
	P->pid = pid;
	Psym_init(P);

	waitpid(pid, &status, 0);
	if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
		rc = EDEADLK;
		_dprintf ("Pcreate: post-fork sync with PID %i failed\n",
		    (int)pid);
		goto bad;
	}
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEEXEC);
	ptrace(PTRACE_CONT, pid, 0, 0);

	waitpid(pid, &status, 0);
	if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP ||
	    status>>8 != (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {
		rc = ENOENT;
		_dprintf ("Pcreate: exec of %s failed\n", file);
		goto bad;
	}

	/*
	 * Initialize the memfd, now we have exec()ed.
	 */
	P->memfd = -1;
	if (Pmemfd(P) == -1)	{		/* populate ->memfd */
		/* error already reported */
		rc = errno;
		goto bad;
	}

	/*
	 * The process is now stopped, waiting in execve() until resumed.
	 */

	snprintf(procname, sizeof (procname), "%s/%d/exe",
	    procfs_path, P->pid);

	if ((P->elf64 = process_elf64(P, procname)) < 0) {
		/* error already reported */
		rc = errno;
		goto bad;
	}

	*perr = 0;
        return P;

bad:
	(void) kill(pid, SIGKILL);
	Pfree(P);
	*perr = rc;
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
 * perr:	pointer to error return code.
 */
struct ps_prochandle *
Pgrab(pid_t pid, int *perr)
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
	P->state = PS_RUN;
	P->pid = pid;
	P->detach = 1;
	Psym_init(P);
	P->bkpts = calloc(BKPT_HASH_BUCKETS, sizeof (struct bkpt_t *));
	if (!P->bkpts) {
		fprintf(stderr, "Out of memory initializing breakpoint hash\n");
		exit(1);
	}
	P->memfd = -1;
	if (Pmemfd(P) == -1)		/* populate ->memfd */
		/* error already reported */
		goto bad;

	Pgrabbing = 1;
	*perr = Ptrace(P, 1);
	Pgrabbing = 0;

	if (*perr < 0) {
		Pfree(P);
		return NULL;
	}
	snprintf(procname, sizeof (procname), "%s/%d/exe",
	    procfs_path, P->pid);

	if ((P->elf64 = process_elf64(P, procname)) < 0)
		/* error already reported */
		goto bad;

	return P;
bad:
	*perr = errno;
	Pfree(P);
	return NULL;
}

/*
 * Free a process control structure.
 *
 * Close the file descriptors but don't do the Prelease logic, nor replace
 * breakpoints with their original content.  (Thus, extremely internal.)
 */
static void
Pfree(struct ps_prochandle *P)
{
	Pclose(P);
	Psym_free(P);

	free(P->auxv);

	free(P->bkpts);
	free(P);
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
 * Clients must not close this file descriptor, nor use it after the process is
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

	strcpy(fname, "mem");
	if ((P->memfd = open(procname, (O_RDONLY|O_EXCL))) < 0) {
		_dprintf("Pmemfd: failed to open %s: %s\n",
		    procname, strerror(errno));
		return (-1);
	}

	fcntl(P->memfd, F_SETFD, FD_CLOEXEC);
	return (P->memfd);
}

/*
 * Release the process.  Free the process control structure.
 * If kill_it is set, kill the process instead.
 *
 * Note: unlike Puntrace(), this releases *all* outstanding traces, cleans up
 * all breakpoints, and detaches unconditionally.  It's meant to be used if the
 * tracer is dying or completely losing interest in its tracee, not merely if it
 * doesn't want to trace it right now.
 */
void
Prelease(struct ps_prochandle *P, boolean_t kill_it)
{
	if (!P)
		return;

	bkpt_flush(P, FALSE);

	if (P->state == PS_DEAD) {
		_dprintf("Prelease: releasing handle %p of dead pid %d\n",
		    (void *)P, (int)P->pid);
		Pfree(P);
		return;
	}

	_dprintf("Prelease: releasing handle %p pid %d\n",
	    (void *)P, (int)P->pid);

	if (kill_it)
		kill(P->pid, SIGKILL);
	else
		ptrace(PTRACE_DETACH, (int)P->pid, 0, 0);

	Pfree(P);
}

/*
 * Wait for the process to stop for any reason, possibly blocking.
 *
 * (A blocking wait will be automatically followed by as many nonblocking waits
 * as are necessary to drain the queue of requests.)
 *
 * Returns the number of state changes processed, or -1 on error.
 */
int
Pwait(struct ps_prochandle *P, boolean_t block)
{
	int err;
	int num_waits = 0;
	siginfo_t info;

	info.si_pid = 0;
	do
	{
		errno = 0;
		err = waitid(P_PID, P->pid, &info, WEXITED | WSTOPPED |
		    (!block ? WNOHANG : 0));

		if (err == 0)
			break;

		if (errno == ECHILD) {
			P->state = PS_DEAD;
			return (0);
		}

		if (errno != EINTR) {
			_dprintf("Pwait: error waiting: %s\n", strerror(errno));
			return (-1);
		}
	} while (errno == EINTR);

	/*
	 * WNOHANG and nothing happened?
	 */
	if (info.si_pid == 0)
		return (0);

	switch (info.si_code) {
	case CLD_EXITED:
	case CLD_KILLED:
	case CLD_DUMPED:
		P->state = PS_DEAD;
		break;
	case CLD_STOPPED:
	case CLD_TRAPPED: {
		long ip;

		/*
		 * Exit under ptrace() monitoring.
		 */
		if (WIFEXITED(info.si_status)) {
			_dprintf("%i: exited with exitcode %i\n", P->pid,
			    WEXITSTATUS(info.si_status));

			P->state = PS_DEAD;
			break;
		}

		/*
		 * Signal receipt.  If the signal is not SIGSTOP under ptrace(),
		 * resend the signal and continue immediately.  (If this is a
		 * group-stop, this will have no effect. Under ptrace(), the
		 * child is already stopped at this point.)
		 */
		if (WIFSTOPPED(info.si_status)) {
			_dprintf("%i: child got stopping signal %i\n", P->pid,
			    WSTOPSIG(info.si_status));
			if (P->ptraced && WSTOPSIG(info.si_status) != SIGSTOP)
				ptrace(PTRACE_CONT, P->pid, NULL,
				    WSTOPSIG(info.si_status));

			P->state = PS_STOP;
			break;
		}

		/*
		 * Ordinary signal injection, or a terminating signal. If this
		 * is not a SIGSTOP, reinject it.  (SIGTRAP is handled further
		 * down.)
		 */
		if ((WIFSIGNALED(info.si_status)) &&
		    (WTERMSIG(info.si_status) != SIGTRAP)) {

			if (!P->ptraced) {
				_dprintf("%i: terminated by signal %i\n",
				    P->pid, WTERMSIG(info.si_status));
				P->state = PS_DEAD;
			}
			else if (WTERMSIG(info.si_status) != SIGSTOP)
				ptrace(PTRACE_CONT, P->pid, NULL,
				    WTERMSIG(info.si_status));
			else
				P->state = PS_TRACESTOP;

			break;
		}

		/*
		 * Other ptrace() traps are generally unexpected unless some
		 * breakpoints are active.  We can only get this when
		 * ptrace()ing, so we know it must be valid to call ptrace()
		 * ourselves.
		 */

		P->state = PS_TRACESTOP;
		if (P->num_bkpts == 0) {
			_dprintf("Pwait: Unexpected ptrace() trap: si_code %i, si_status %i\n",
			    info.si_code, info.si_status);
			return (-1);
		}

		/*
		 * TRACEEXEC trap.  Our breakpoints are gone, our auxv has
		 * changed, our memfd needs reopening, any (internal) exec()
		 * handler should be called, and we need to seize control again.
		 * We also need to arrange to trap the *next* exec(), then
		 * resume.
		 *
		 * We are always tracing the thread group leader, so most of the
		 * complexity around execve() tracing goes away.
		 */
		if (info.si_status == (SIGTRAP | PTRACE_EVENT_EXEC << 8)) {

			_dprintf("%i: exec() detected, resetting...\n", P->pid);
			Pclose(P);
			if (Pmemfd(P) == -1)
				_dprintf("%i: Cannot reopen memory\n", P->pid);
			bkpt_flush(P, TRUE);

			free(P->auxv);
			P->auxv = NULL;
			P->nauxv = 0;
			P->r_debug_addr = 0;
			P->ptrace_count = 1;
			P->ptraced = TRUE;

			if (P->exec_handler)
				P->exec_handler(P);

			Puntrace(P, 0);

			break;
		}

		/*
		 * Breakpoint trap.  The kernel translates between 32- and 64-bit
		 * regsets for us, but does not helpfully adjust for the fact
		 * that the trap address needs adjustment on some platforms
		 * before it will correspond to the address of the breakpoint.
		 *
		 * If we are in the midst of processing a breakpoint, we already
		 * know our breakpoint address.
		 */

		ip = P->tracing_bkpt;
		if (ip == 0) {
			errno = 0;
			ip = ptrace(PTRACE_PEEKUSER, P->pid, PLAT_IP * sizeof (long));
			if (errno != 0) {
				_dprintf("Pwait: Unexpected ptrace (PTRACE_PEEKUSER) "
				    "error: %s\n", strerror(errno));
				return(-1);
			}
			ip += plat_trap_ip_adjust;
		}

		P->state = bkpt_handle(P, ip);

		break;
	}
	case CLD_CONTINUED:
		P->state = PS_RUN;
		break;

	default:	/* corrupted state */
		_dprintf("Pwait: unknown si_code: %li\n", (long int) info.si_code);
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Now repeatedly loop, processing more waits until none remain.
	 */
	if (block != 0) {
		int this_wait;
		do {
			this_wait = Pwait(P, 0);
			num_waits += this_wait;
		} while (this_wait > 0);
	}

	return (num_waits + 1);
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
 * Grab a ptrace(), unless one is already grabbed: increment the ptrace count.
 * Regardless, the process will be PS_TRACESTOP on return if 'stopped'.
 *
 * Returns the previous stop state, or, if <0, an error code.
 *
 * Failure to trace is considered an error unless called from within Pgrab().
 */
int
Ptrace(struct ps_prochandle *P, int stopped)
{
	int err;
	int status;

	P->ptrace_count++;
	if (P->ptraced) {
		int state;

		/*
		 * In this case, we must take care that anything already queued
		 * for Pwait() is correctly processed, before demanding a stop.
		 * We should also not try to stop something that is already
		 * stopped, nor try to resume it.
		 */
		Pwait(P, 0);
		if ((!stopped) || (P->state == PS_TRACESTOP))
			return P->state;

		state = P->state;
		kill(P->pid, SIGSTOP);
		Pwait(P, 1);
		return state;
	}

	err = ptrace(PT_ATTACH, P->pid, 0, 0);

	if ((!Pgrabbing) && (err < 0))
		goto err2;

	if (err == 0)
		P->ptraced = TRUE;

	err = P->state;

	if (waitpid(P->pid, &status, 0) == -1)
		goto err2;
	else if (WIFSTOPPED(status) == 0)
		goto err2;
	P->state = PS_TRACESTOP;

	if (!stopped) {
		int err;

		err = ptrace(PTRACE_CONT, P->pid, 0, 0);
		if (err < 0)
			goto err;
		Pwait(P, 1);
	}

	return err;
err:
	err = errno;
err2:
	P->ptrace_count--;
	if (err != ECHILD)
		return err;
	else
		return ESRCH; /* for a clearer message */
}

/*
 * Ungrab a ptrace(). If none are left, set running, or detach if P->detach.
 * If stuck at a breakpoint, resume the process.
 *
 * If nonzero, 'state' is the previous stop state returned from the matching
 * call to Ptrace().  If a state other than PS_RUN is stipulated, we do not
 * resume, but in fact remain under ptrace().  This is so that
 * Ptrace()/Puntrace() pairs called from within a breakpoint trap function
 * properly and do not affect the state of the system. In effect, Puntrace()
 * should cause a resume only if Ptrace() caused a stop.
 *
 * Lots of P->state comparisons are needed here, because a PTRACE_CONT is only
 * valid if the processe was stopped beforehand: otherwise, you get -ESRCH,
 * which is helpfully indistinguishable from the error when the process is dead.
 *
 * A state <0 is considered a sign that the original Ptrace() call failed, and
 * silently does nothing.
 */
void
Puntrace(struct ps_prochandle *P, int state)
{
	/*
	 * Protect against unbalanced Ptrace()/Puntrace().
	 */
	if ((!P->ptraced) || (state < 0))
		return;

	P->ptrace_count--;

	/*
	 * Still under Ptrace(), or a stay in stop state requested, or stuck in
	 * stop state by request of an earlier breakpoint handler?  OK, nothing
	 * needs doing, except possibly a resume, using Pbkpt_continue() so as
	 * to do the right thing if a breakpoint is outstanding.
	 */
	if (P->ptrace_count || P->bkpt_halted ||
	    ((state != 0) && (state != PS_RUN))) {
		if ((state == PS_RUN) && (P->state == PS_TRACESTOP)) {
			P->state = state;
			_dprintf("%i: Continuing because previous state was "
			    "PS_RUN\n", P->pid);
			Pbkpt_continue(P);
		}
		return;
	}

	/*
	 * Continue the process, or detach it if requested, no breakpoints
	 * are outstanding, and no rd_agent is active.
	 */
	if ((!P->detach) || P->rap || (P->num_bkpts > 0)) {
		if (P->state == PS_TRACESTOP) {
			_dprintf("%i: Continuing.\n", P->pid);
			Pbkpt_continue(P);
			Pwait(P, 0);
		}

	} else {
		_dprintf("%i: Detaching\n", P->pid);
		P->state = PS_RUN;
		P->ptraced = FALSE;
		if ((ptrace(PTRACE_DETACH, P->pid, 0, 0) < 0) &&
		    (errno == ESRCH))
			P->state = PS_DEAD;
	}
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
 * The breakpoint handler should return nonzero to remain stopped at this
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
	bkpt_t *bkpt = bkpt_by_addr(P, addr, FALSE);
	uint_t h = addr % BKPT_HASH_BUCKETS;
	int orig_state;
	int err;

	/*
	 * Already present? Just tweak it.
	 */
	if (bkpt) {
		if (bkpt->bkpt_handler.bkpt_cleanup)
			bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);
		bkpt->bkpt_handler.bkpt_handler = bkpt_handler;
		bkpt->bkpt_handler.bkpt_cleanup = bkpt_cleanup;
		bkpt->bkpt_handler.bkpt_data = data;
		bkpt->after_singlestep = after_singlestep;
		return 0;
	}

	orig_state = Ptrace(P, 1);
	if (orig_state < 0) {
		err = orig_state;
		goto err2;
	}

	/*
	 * Allocate and poke in a new breakpoint.
	 */

	bkpt = malloc(sizeof (struct bkpt));
	if (!bkpt)
		goto err;

	memset(bkpt, 0, sizeof (struct bkpt));
	bkpt->bkpt_handler.bkpt_handler = bkpt_handler;
	bkpt->bkpt_handler.bkpt_cleanup = bkpt_cleanup;
	bkpt->bkpt_handler.bkpt_data = data;
	bkpt->after_singlestep = after_singlestep;
	bkpt->bkpt_addr = addr;

	errno = 0;
	bkpt->orig_insn = ptrace(PTRACE_PEEKTEXT, P->pid,
	    addr, 0);
	if (errno != 0) {
		free(bkpt);
		goto err;
		return errno;
	}
	if (ptrace(PTRACE_POKETEXT, P->pid, addr,
		mask_bkpt(bkpt->orig_insn)) < 0) {
		free(bkpt);
		goto err;
		return errno;
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
	Puntrace(P, orig_state);
	return 0;
err:
	err = errno;
err2:
	_dprintf("%i: Cannot add breakpoint on %lx: %s\n", P->pid,
	    addr, strerror(errno));
	Puntrace(P, orig_state);
	return err;
}

/*
 * Remove a breakpoint on a given address, if one exists there.
 *
 * The cleanup handler, if any, is called.
 */
void
Punbkpt(struct ps_prochandle *P, uintptr_t addr)
{
	bkpt_t *bkpt;
	int orig_state;

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

	orig_state = Ptrace(P, 1);
	if (orig_state < 0)
		_dprintf("%i: Unexpected error %s ptrace()ing to remove "
		    "breakpoint.", P->pid, strerror(orig_state));

	Pwait(P, 0);
	bkpt = bkpt_by_addr(P, addr, TRUE);
	if (!bkpt) {
		_dprintf("%i: Punbkpt() called with %lx, which is not a known "
		    "breakpoint.\n", P->pid, addr);
		return;
	}

	P->num_bkpts--;

	/*
	 * If we are not singlestepping past this breakpoint now, we have to
	 * poke the old content back in.  Otherwise, we just have to adjust our
	 * IP address and resume (and the resumption is done automatically for
	 * us when we Puntrace() in any case).
	 */
	if (P->tracing_bkpt != bkpt->bkpt_addr) {
		uintptr_t insn;
		/*
		 * Only overwrite the breakpoint insn if it is still a
		 * breakpoint.  If it has changed (perhaps due to a new text
		 * section being mapped in), do nothing.
		 */
		errno = 0;
		insn = ptrace(PTRACE_PEEKTEXT, P->pid, bkpt->bkpt_addr, 0);

		if (errno == 0 && insn == mask_bkpt(insn) &&
		    ptrace(PTRACE_POKETEXT, P->pid,
			bkpt->bkpt_addr, bkpt->orig_insn) < 0)
			switch (errno) {
			case ESRCH:
				P->state = PS_DEAD;
				return;
			case EIO:
			case EFAULT:
				/* The address in the child has disappeared. */
			case 0: break;
			default:
				_dprintf("%i: Unknown error removing breakpoint:"
				    "%s\n", P->pid, strerror(errno));
			}
	} else {
		if (ptrace(PTRACE_POKEUSER, P->pid, PLAT_IP * sizeof (long),
			P->tracing_bkpt) < 0)
			switch (errno) {
			case ESRCH:
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
		P->singlestepped = 0;
		P->bkpt_halted = 0;
		orig_state = PS_RUN;
	}
	if (bkpt->bkpt_handler.bkpt_cleanup)
		bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);

	free(bkpt);

	Puntrace(P, orig_state);

	_dprintf("%i: Removed breakpoint on %lx\n", P->pid, addr);
}

/*
 * Discard breakpoint state.
 *
 * Done when the child process is dead or being released or its address space
 * has vanished due to an exec().
 *
 * Rendered more complicated by the need to do local breakpoint cleanup even if
 * the process is gone.  (We cannot use Punbkpt() because the process might have
 * exec()ed, and we do not want to continue it, nor modify the address of
 * breakpoints that can no longer exist within it.)
 */
static void
bkpt_flush(struct ps_prochandle *P, int gone) {
	size_t i;

	for (i = 0; i < BKPT_HASH_BUCKETS; i++) {
		bkpt_t *bkpt;
		bkpt_t *old_bkpt = NULL;

		for (bkpt = P->bkpts[i]; bkpt != NULL;
		     old_bkpt = bkpt, bkpt = bkpt->bkpt_next) {
			if (old_bkpt != NULL) {
				if (!gone)
					Punbkpt(P, old_bkpt->bkpt_addr);
				else {
					bkpt_t *bkpt = bkpt_by_addr(P, old_bkpt->bkpt_addr, TRUE);

					if (bkpt->bkpt_handler.bkpt_cleanup)
						bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);

					free(bkpt);
				}
			}
		}

		if (old_bkpt != NULL) {
			if (!gone)
				Punbkpt(P, old_bkpt->bkpt_addr);
			else {
				bkpt_t *bkpt = bkpt_by_addr(P, old_bkpt->bkpt_addr, TRUE);

				if (bkpt->bkpt_handler.bkpt_cleanup)
					bkpt->bkpt_handler.bkpt_cleanup(bkpt->bkpt_handler.bkpt_data);

				free(bkpt);
			}
		}
	}

	P->tracing_bkpt = 0;
	P->singlestepped = 0;
	P->bkpt_halted = 0;
	P->num_bkpts = 0;
}

/*
 * Handle a breakpoint.
 *
 * Returns a process state.
 */
static int
bkpt_handle(struct ps_prochandle *P, uintptr_t addr)
{
	/*
	 * Either we have stopped at an address we know, or we are
	 * singlestepping past one of our breakpoints, or this is an unknown
	 * breakpoint.  If the latter, just return in TRACESTOP state, and hope
	 * our caller knows what to do.
	 */
	if (P->tracing_bkpt == addr)
		return bkpt_handle_singlestep(P,
		    bkpt_by_addr(P, P->tracing_bkpt, FALSE));
	else {
		bkpt_t *bkpt = bkpt_by_addr(P, addr, FALSE);

		if (!bkpt) {
			_dprintf("%i: No breakpoint found at %lx, remaining in "
			    "trace stop.\n", P->pid, addr);
			return PS_TRACESTOP;
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
				orig_insn = ptrace(PTRACE_PEEKTEXT,
				    P->pid, bkpt->bkpt_addr, 0);
				ptrace(PTRACE_POKETEXT, P->pid,
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
	if (ptrace(PTRACE_POKETEXT, P->pid,
		bkpt->bkpt_addr, bkpt->orig_insn) < 0)
		switch (errno) {
		case ESRCH:
			return PS_DEAD;
		case 0:
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
	P->singlestepped = 0;

	if (!bkpt->after_singlestep) {
		int state = bkpt->bkpt_handler.bkpt_handler(bkpt->bkpt_addr,
		    bkpt->bkpt_handler.bkpt_data);

		if (state != PS_RUN) {
			P->bkpt_halted = 1;
			return state;
		}

		/*
		 * Look up the breakpoint anew, in case the handler erased it.
		 *
		 * If it did, Punbkpt() will have continued for us.
		 */
		bkpt = bkpt_by_addr(P, P->tracing_bkpt, FALSE);
		if (!bkpt)
			return PS_RUN;
	}

	return Pbkpt_continue_internal(P, bkpt);
}

/*
 * Continue a process stopped at a breakpoint.
 */
void
Pbkpt_continue(struct ps_prochandle *P)
{
	bkpt_t *bkpt = bkpt_by_addr(P, P->tracing_bkpt, FALSE);

	if (!P->ptraced)
		return;

	P->bkpt_halted = 0;
	/*
	 * We don't know where we are.  We might not be stopped at a breakpoint
	 * at all: we might not even be ptracing the process (or it might have
	 * done something that prohibits our tracing it, like exec()ed a setuid
	 * process).  Just issue a continue.
	 */
	if (P->tracing_bkpt == 0 || !bkpt) {
		if (ptrace(PTRACE_CONT, P->pid, 0, 0) < 0) {
			if (errno == ESRCH) {
				if ((kill(P->pid, 0) < 0) && errno == ESRCH)
					P->state = PS_DEAD;
			}
			/*
			 * Since we must have an outstanding Ptrace() anyway,
			 * assume that an EPERM means that this process is not
			 * stopped: it cannot mean that we aren't allowed to
			 * ptrace() it.
			 */
			if (errno != EPERM) {
				_dprintf("%i: Unexpected error resuming: %s\n",
				    P->pid, strerror(errno));
				return;
			}
		}
		P->state = PS_RUN;
		return;
	}

	/*
	 * We are at a breakpoint locus.
	 */
	P->state = Pbkpt_continue_internal(P, bkpt);
}

/*
 * Continue a process stopped at a given breakpoint locus, resetting the
 * instruction pointer and issuing a singlestep if needed, or calling
 * bkpt_handle_singlestep() to call the breakpoint handler, poke the breakpoint
 * instruction back in, and resume execution.
 */
static int
Pbkpt_continue_internal(struct ps_prochandle *P, bkpt_t *bkpt)
{
	if (!P->singlestepped) {
		P->singlestepped = 1;

		if ((ptrace(PTRACE_POKEUSER, P->pid, PLAT_IP * sizeof (long),
			    bkpt->bkpt_addr) == 0) &&
		    (ptrace(PTRACE_SINGLESTEP, P->pid, 0, 0) == 0))
			return PS_RUN;
		else if (errno == ESRCH)
			return PS_DEAD;
		else
			return PS_TRACESTOP;
	}

	return bkpt_handle_singlestep(P, bkpt);
}

/*
 * Single-step past a breakpoint.  When called, we are known to have completed a
 * singlestep over the specified breakpoint and to be in trace-stop state.
 *
 * Returns a process state.
 */
static int
bkpt_handle_singlestep(struct ps_prochandle *P, bkpt_t *bkpt)
{
	int state = PS_RUN;
	int orig_insn;

	if (bkpt->after_singlestep)
		state = bkpt->bkpt_handler.bkpt_handler(bkpt->bkpt_addr,
		    bkpt->bkpt_handler.bkpt_data);

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
	 * Look up the breakpoint anew, in case the handler erased it.
	 *
	 * If it did, Punbkpt() will have continued for us.
	 */
	bkpt = bkpt_by_addr(P, bkpt->bkpt_addr, FALSE);
	if (!bkpt)
		return PS_RUN;

	/*
	 * The handler wants us to run. Resume.
	 *
	 * Always re-peek the original instruction, in case of self-modifying
	 * code.  If it fails, hold on to the old one.
	 */

	errno = 0;
	orig_insn = ptrace(PTRACE_PEEKTEXT, P->pid, bkpt->bkpt_addr, 0);
	if (errno != 0)
		_dprintf("Unexpected error re-peeking original instruction "
		    "on PID %i: %s\n", P->pid, strerror(errno));
	else
		bkpt->orig_insn = orig_insn;

	/*
	 * The error handling here is verbose and annoying, but unavoidable, as
	 * the process could be SIGKILLed at any time, even between these
	 * ptrace() calls.
	 */

	if (ptrace(PTRACE_POKETEXT, P->pid, bkpt->bkpt_addr,
		mask_bkpt(orig_insn)) < 0)
		switch (errno) {
		case ESRCH:
			return PS_DEAD;
		case 0:
			break;
		case EIO:
		case EFAULT:
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
	P->singlestepped = 0;

	if (ptrace(PTRACE_CONT, P->pid, 0, 0) < 0) {
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

/*
 * Read from process, doing endianness and size conversions.
 */
ssize_t
Pread_scalar(struct ps_prochandle *P,
    void *buf,			/* caller's buffer */
    size_t nbyte,		/* number of bytes to read */
    size_t nscalar,		/* scalar size on this platform */
    uintptr_t address)		/* address in process */
{
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
		_dprintf("Pread_scalar(): attempt to read %lu bytes from PID %i at "
		    "address %lx read fewer bytes than expected.\n", nbyte,
		    P->pid, address);
		return(-1);
	}

	switch (nscalar) {
	case sizeof(conv.b1): memcpy(buf, &conv.b1, nscalar); break;
	case sizeof(conv.b2): memcpy(buf, &conv.b2, nscalar); break;
	case sizeof(conv.b4): memcpy(buf, &conv.b4, nscalar); break;
	case sizeof(conv.b8): memcpy(buf, &conv.b8, nscalar); break;
	default: _dprintf("Pread_scalar(): Attempt to read into a scalar of "
	    "%lu bytes is not supported.\n", nscalar);
		return(-1);
	}

	return nbyte;
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
 * Set a handler for exec()s.
 */
void
set_exec_handler(struct ps_prochandle *P, exec_handler_fun handler)
{
	P->exec_handler = handler;
}
