/*
 * rtld debugging library.
 */

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
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <inttypes.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <elf.h>
#include <link.h>
#include <rtld_db.h>
#include <rtld_offsets.h>

#include "libproc.h"
#include "Pcontrol.h"

/*
 * Read FIELD from STRUCTURE into BUF.  STRUCTURE is rooted at address ADDR in
 * process P; its offsets array is OFFSETS.
 */
#define read_scalar_child(P, buf, addr, offsets, structure, field)	\
	Pread_scalar(P, buf, offsets[offsetof(struct structure, field)].size[P->elf64], \
	    sizeof(((struct structure *)0)->field),			\
	    addr + offsets[offsetof(struct structure, field)].offset[P->elf64])

/*
 * Tripped when ld.so hits our breakpoint on r_brk (used for consistency
 * checking and rtld event reporting).
 */
static int rd_brk_trap(uintptr_t addr, void *rd_data);

/*
 * Tripped when the child process hits our breakpoint on the start address.
 */
static int rd_start_trap(uintptr_t addr, void *rd_data);

/*
 * Try to get the address of r_brk, if not already known: return that address,
 * or 0 if none.
 */
static uintptr_t r_brk(rd_agent_t *rd);

/* Internal functions. */

/*
 * Read dynamic linker consistency information out of the traced child.
 *
 * (A value !RD_CONSISTENT does not necessarily indicate that we *are*
 * inconsistent, merely that we *may* be: we may be stopped on the r_brk
 * breakpoint, and consistent.  To ensure consistency, use
 * rd_ldso_consistent_begin()/rd_ldso_consistent_end(), which handle this
 * ambiguity.)
 */
static int
rd_ldso_consistency(rd_agent_t *rd)
{
	int r_state;

	if (read_scalar_child(rd->P, &r_state, rd->P->r_debug_addr,
		r_debug_offsets, r_debug, r_state) <= 0) {

		static int warned = 0;
		/*
		 * Read failed? Warn, but assume consistent (because we must, or
		 * we will block forever).
		 */
		if (!warned)
			_dprintf("Cannot read r_state to determine consistency "
			    "of child with PID %i.\n", rd->P->pid);
		warned = 1;
		return RD_CONSISTENT;
	}

	return r_state;
}

/*
 * Wait for the dynamic linker to be in a consistent state.
 *
 * On exit, the process may be running, or it may be stopped: but until
 * rd_ldso_consistent_end() is called, it will be in a consistent state (and may
 * transition into stopped state at any time to enforce this).
 *
 * Pwait()ing on a process between an rd_ldso_consistent_begin() and
 * rd_ldso_consistent_end() may deadlock.
 */
static int
rd_ldso_consistent_begin(rd_agent_t *rd)
{
	int err;

	/*
	 * Begin ptracing, and record the run state of the process before
	 * consistency enforcement began.
	 */
	Ptrace(rd->P, 0);
	rd->prev_state = rd->P->state;

	/*
	 * If consistent state monitoring is not yet active, reset the marker
	 * that signals that a state transition was detected.  Do this before we
	 * start monitoring.
	 */
	if (!rd->no_inconsistent)
		rd->ic_transitioned = 0;

	/*
	 * Bar transition of the dynamic linker into an inconsistent state (if
	 * it is currently in a consistent state), and arrange to signal when
	 * a transition is detected.
	 */
	rd->no_inconsistent++;

	/*
	 * If needed, set up the breakpoint to detect transitions into
	 * consistent/inconsistent states.
	 */
	if (rd->no_inconsistent == 1 && !rd->rd_monitoring) {
		err = Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap, NULL, rd);
		rd->rd_monitoring = TRUE;
		if (err != 0) {
			rd->no_inconsistent--;
			return err;
		}
	}

	/*
	 * If we are currently in an inconsistent state, wait until we
	 * transition into a consistent state, or die.  (If we are not currently
	 * in an inconsistent state, a transition at this point will halt us
	 * immediately before the link maps become inconsistent, though
	 * rd_ldso_consistency() will not report RD_CONSISTENT in this case.)
	 */
	if (rd_ldso_consistency(rd) != RD_CONSISTENT) {

		_dprintf("%i: link maps inconsistent: waiting for "
		    "transition.\n", rd->P->pid);

		/*
		 * If we die inside the dynamic linker, drop out.  If we
		 * silently become consistent, drop out.  If someone drops a
		 * breakpoint somewhere inside the dynamic linker, we will will
		 * return with inconsistent link maps.  Don't do that.
		 */
		while (!rd->ic_transitioned && rd->P->state == PS_RUN &&
		    rd_ldso_consistency(rd) != RD_CONSISTENT)
			Pwait(rd->P, TRUE);
	}

	return 0;
}

static void
rd_ldso_consistent_end(rd_agent_t *rd)
{
	/*
	 * Protect against unbalanced calls.
	 */
	if (!rd->no_inconsistent)
		return;

	rd->no_inconsistent--;

	if (rd->no_inconsistent)
		return;

	/*
	 * Turn off the breakpoint, if it's not being used for anything else.
	 */
	if (!rd->rd_event_fun) {
		Punbkpt(rd->P, rd->r_brk_addr);
		rd->rd_monitoring = FALSE;
	}

	/*
	 * End tracing. If the child was stopped to retain consistency, this
	 * will resume it.
	 */
	Puntrace(rd->P, rd->prev_state);
}

/*
 * Tripped after ld.so hits our breakpoint on r_brk and singlesteps past it
 * (used for consistency checking and rtld event reporting).
 */
static int
rd_brk_trap(uintptr_t addr, void *rd_data)
{
	rd_agent_t *rd = rd_data;
	int consistency = rd_ldso_consistency(rd);

	rd->ic_transitioned = 1;

	/*
	 * Block if inconsistent state transitions are barred.
	 */
	_dprintf("%i: rd_brk breakpoint hit, %s\n", rd->P->pid,
	    consistency == RT_CONSISTENT ? "consistent" : "inconsistent");

	if ((rd->no_inconsistent) && (consistency != RT_CONSISTENT))
		return PS_TRACESTOP;

	/*
	 * If we are doing event monitoring, call the callback.
	 */
	if (rd->rd_event_fun) {
		rd_event_msg_t msg;
		msg.type = RD_DLACTIVITY;
		msg.state = consistency;

		_dprintf("%i: rtld activity event fired.\n", rd->P->pid);
		rd->rd_event_fun(rd, &msg, rd->rd_event_data);
	}

	return PS_RUN;
}

/*
 * Try to get the address of r_brk, if not already known: return that address,
 * or 0 if none.
 *
 * In dynamically linked programs, we can almost always get this successfully,
 * as long as the dynamic linker has finished initializing: but in statically
 * linked programs, this is lazily initialized when dlopen() is called for the
 * first time.  So we must initialize it later.
 *
 * All entry points to rtld_db which directly or indirectly access r_brk_addr
 * must test that r_brk(rd) != 0 first!
 */
static uintptr_t
r_brk(rd_agent_t *rd)
{
	static int warned = 0;
	uintptr_t r_debug_addr;
	int r_version = -1;

	if (rd->r_brk_addr)
		return rd->r_brk_addr;

	r_debug_addr = r_debug(rd->P);

	if (r_debug_addr == -1)
		return 0;

	/*
	 * Check its version.
	 */
	if ((read_scalar_child(rd->P, &r_version, r_debug_addr,
		    r_debug_offsets, r_debug, r_version) <= 0) ||
	    (r_version > 1)) {
		if (!warned)
			_dprintf("%i: r_version %i unsupported.\n",
			    rd->P->pid, r_version);
		warned = 1;
		return 0;
	}

	if (r_version == 0)
		return 0;

	/*
	 * Get the address of the r_brk dynamic linker breakpoint.
	 */
	if ((read_scalar_child(rd->P, &rd->r_brk_addr, r_debug_addr,
		    r_debug_offsets, r_debug, r_brk) <= 0) ||
	    (rd->r_brk_addr == 0)) {
		if (!warned)
			_dprintf("%i: Cannot determine dynamic linker "
			    "breakpoint address.\n", rd->P->pid);
	}

	return rd->r_brk_addr;
}

/*
 * Tripped when we hit our breakpoint on the user entry point.
 *
 * At this point, in a dynamically linked program, the dynamic linker is
 * initialized.
 */
static int
rd_start_trap(uintptr_t addr, void *rd_data)
{
	rd_agent_t *rd = rd_data;
	uintptr_t r_debug_addr = r_debug(rd->P);

	/*
	 * Can't find r_debug, even though ld.so has initialized?  Abandon all
	 * attempts to track dynamic linker state: we will pick things up again
	 * when another exec() happens.
	 */

	if (r_debug_addr == -1) {
		_dprintf("Cannot initialize rd_agent for PID %i: no r_debug.\n",
		    rd->P->pid);
		goto done;
	}

	r_brk(rd);

	/*
	 * Reactivate the rd_monitoring breakpoint, if it should be active (e.g.
	 * if it was active before an exec()).  Silently disable monitoring if
	 * we cannot reactivate the breakpoint.
	 */
	if (rd->rd_monitoring)
		if (Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap,
			NULL, rd) != 0)
			rd->rd_monitoring = FALSE;

done:
	_dprintf("Hit start trap, r_brk is %lx; removing start trap breakpoint "
	    "on %lx\n", rd->r_brk_addr, addr);
	Punbkpt(rd->P, addr);
	return PS_RUN;
}

/*
 * Carry the rd_agent over an exec().  Drop a breakpoint on the process entry
 * point.
 */
static void
rd_exec_handler(struct ps_prochandle *P)
{
	P->rap->r_brk_addr = 0;
	if (Pbkpt(P, Pgetauxval(P, AT_ENTRY), FALSE,
		rd_start_trap, NULL, P->rap) != 0)
		_dprintf("%i: cannot drop breakpoint at process "
		    "start.\n", P->pid);
}

/* API functions. */ 

/*
 * Initialize a rd_agent.
 */
rd_agent_t *
rd_new(struct ps_prochandle *P)
{
	rd_agent_t *rd = calloc(sizeof (struct rd_agent), 1);
	int r_version = 0;
	int oldstate;
	uintptr_t r_debug_addr = r_debug(P);

	Pwait(P, 0);
	if (rd == NULL)
		return (NULL);
	/*
	 * Protect against multiple calls.
	 */
	if (P->rap)
		return (P->rap);

	rd->P = P;
	oldstate = Ptrace(P, 1);

	/*
	 * Can't find r_debug?  This is hopeless.  Quite possibly the process is
	 * dead, or a stripped statically linked binary.
	 */
	if (r_debug_addr == -1) {
		if (P->state == PS_DEAD)
			_dprintf ("%i: Cannot initialize rd_agent: "
			    "process is dead.\n", rd->P->pid);
		else
			_dprintf("%i: Cannot initialize rd_agent: no "
			    "r_debug.\n", rd->P->pid);
		goto err;
	}

	/*
	 * Check its version.  An _r_debug address or version of zero means that
	 * the dynamic linker has not yet initialized.
	 */
	if (r_debug_addr != 0 &&
	    ((read_scalar_child(rd->P, &r_version, r_debug_addr,
		    r_debug_offsets, r_debug, r_version) <= 0) ||
		(r_version > 1))) {
		_dprintf("%i: r_version %i unsupported.\n", rd->P->pid,
		    r_version);
		goto err;
	}

	/*
	 * If the dynamic linker is initialized, get the address of the r_brk
	 * dynamic linker breakpoint.  (We cannot trust this otherwise, because
	 * it will move around when the dynamic linker relocates itself.)
	 */

	if (r_version > 0) {
		if ((read_scalar_child(rd->P, &rd->r_brk_addr, r_debug_addr,
			    r_debug_offsets, r_debug, r_brk) <= 0) ||
		    (rd->r_brk_addr == 0)) {
			_dprintf("%i: Cannot determine dynamic linker "
			    "breakpoint address.\n", rd->P->pid);
			goto err;
		}
	} else
		/*
		 * Dynamic linker not yet initialized.  Drop a breakpoint on the
		 * entry address, so we can initialize it then.  If this is a
		 * statically linked binary, this is not sufficient: _r_debug is
		 * initialized at an unknown distant future point, when
		 * the first shared library is loaded.  We'll just return
		 * RD_NOMAPS until then. 
		 */

		if (!P->no_dyn &&
		    Pbkpt(rd->P, Pgetauxval(rd->P, AT_ENTRY), FALSE,
			rd_start_trap, NULL, rd) != 0) {
			_dprintf("%i: cannot drop breakpoint at process start.\n",
			    rd->P->pid);
			goto err;
		}

	set_exec_handler(rd->P, rd_exec_handler);
	P->rap = rd;
	Puntrace(P, oldstate);

	_dprintf("%i: Activated rtld_db agent.\n", rd->P->pid);
	return (rd);
err:
	Puntrace(P, oldstate);
	free(rd);
	return (NULL);
}

/*
 * Shut down a rd_agent.
 */
void
rd_delete(rd_agent_t *rd)
{
	/*
	 * Protect against NULL calls, and multiple calls.
	 */
	if (!rd)
		return;

	if (rd->P->rap != rd)
		return;

	while (rd->no_inconsistent)
		rd_ldso_consistent_end(rd);

	rd_event_disable(rd);
	set_exec_handler(rd->P, NULL);

	if (rd->rd_monitoring) {
		Punbkpt(rd->P, rd->r_brk_addr);
		Punbkpt(rd->P, Pgetauxval(rd->P, AT_ENTRY)); /* just in case */
	}
	rd->P->rap = NULL;

	_dprintf("%i: Deactivated rtld_db agent.\n", rd->P->pid);
	free(rd);
}

/*
 * Enable DLACTIVITY monitoring.
 */
rd_err_e
rd_event_enable(rd_agent_t *rd, rd_event_fun fun, void *data)
{
	rd->rd_event_fun = fun;
	rd->rd_event_data = data;

	if (rd->rd_monitoring)
		return RD_OK;

	if (r_brk(rd) == 0)
		return RD_NOMAPS;

	if (Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap, NULL, rd) != 0)
		return RD_ERR;

	_dprintf("%i: enabled rtld activity monitoring.\n", rd->P->pid);

	return RD_OK;
}

/*
 * Disable DLACTIVITY monitoring.
 */
void
rd_event_disable(rd_agent_t *rd)
{
	/*
	 * Tell the event callback that we are shutting down.
	 */

	if (rd->rd_event_fun)
		rd->rd_event_fun(rd, NULL, rd->rd_event_data);

	rd->rd_event_fun = NULL;
	rd->rd_event_data = NULL;
	if (rd->rd_monitoring && rd->no_inconsistent == 0)
		Punbkpt(rd->P, rd->r_brk_addr);

	_dprintf("%i: disabled rtld activity monitoring.\n", rd->P->pid);
}

/*
 * Enable iteration over the link maps.
 *
 * Note: RD_NOMAPS can be returned at any time, even after some maps have been
 * iterated over.
 */
rd_err_e
rd_loadobj_iter(rd_agent_t *rd, rl_iter_f *fun, void *state)
{
	uintptr_t loadobj;
	uintptr_t old_r_brk;

	Pwait(rd->P, 0);
	if (r_brk(rd) == 0) {
		_dprintf("%i: link map iteration failed: maps are not ready.\n",
		    rd->P->pid);
		return RD_NOMAPS;
	}

	if (rd_ldso_consistent_begin(rd) != 0) {
		_dprintf("%i: link map iteration failed: cannot wait for "
		    "consistent state.\n", rd->P->pid);
		return RD_ERR;
	}

	/*
	 * Retest to make sure we haven't exec()ed while waiting for ld.so to
	 * enter a consistent state.  (This is unlikely, but possible:
	 * e.g. another thread could have exec()ed.)
	 */
	if (r_brk(rd) == 0) {
		rd_ldso_consistent_end(rd);
		_dprintf("%i: link map iteration failed: maps are not ready.\n",
		    rd->P->pid);
		return RD_NOMAPS;
	}

	if (read_scalar_child(rd->P, &loadobj, r_debug(rd->P),
		r_debug_offsets, r_debug, r_map) <= 0) {
		rd_ldso_consistent_end(rd);

		_dprintf("%i: link map iteration failed: cannot read link maps "
		    "from child.\n", rd->P->pid);
		return RD_ERR;
	}

	if (loadobj == 0) {
		rd_ldso_consistent_end(rd);

		_dprintf("%i: link map iteration: no maps.\n", rd->P->pid);
		return RD_NOMAPS;
	}

	_dprintf("%i: iterating over link maps.\n", rd->P->pid);
	while (loadobj != 0) {
		rd_loadobj_t obj;
		size_t size;
		size_t offset;

		if ((read_scalar_child(rd->P, &obj.rl_base, loadobj,
			link_map_offsets, link_map, l_addr) <= 0) ||
		    (read_scalar_child(rd->P, &obj.rl_nameaddr, loadobj,
			link_map_offsets, link_map, l_name) <= 0) ||
		    (read_scalar_child(rd->P, &obj.rl_dyn, loadobj,
			link_map_offsets, link_map, l_ld) <= 0))
			goto err;

		/*
		 * The lmid is not public in the <link.h> header, so rather than
		 * use the by-offset indexing, we name the offset explicitly.
		 */
		if (rd->P->elf64) {
			size = L_LMID_64_SIZE;
			offset = L_LMID_64_OFFSET;
		} else {
			size = L_LMID_32_SIZE;
			offset = L_LMID_32_OFFSET;
		}
		if (Pread_scalar(rd->P, &obj.rl_lmident, size,
			sizeof (obj.rl_lmident), loadobj + offset) < 0)
			goto err;

		if (fun(&obj, state) == 0)
			goto err;

		if (read_scalar_child(rd->P, &loadobj, loadobj,
		    link_map_offsets, link_map, l_next) <= 0) {
			rd_ldso_consistent_end(rd);

			_dprintf("%i: link map iteration failed: cannot read link maps "
			    "from child.\n", rd->P->pid);
			return RD_ERR;
		}
	}

	rd_ldso_consistent_end(rd);
	return RD_OK;

err:
	rd_ldso_consistent_end(rd);

	/*
	 * It is possible we have just died or exec()ed in the middle of
	 * iteration.  Pwait() to pick that up.
	 */
	old_r_brk = r_brk(rd);
	Pwait(rd->P, 0);
	if (rd->P->state == PS_DEAD ||
	    r_brk(rd) == 0 || r_brk(rd) != old_r_brk) {
		_dprintf("%i: link map iteration failed: maps are not ready.\n",
		    rd->P->pid);
		return RD_NOMAPS;
	}

	_dprintf("%i: link map iteration: read from child failed.\n", rd->P->pid);
	return RD_ERR;
}
