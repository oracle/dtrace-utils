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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_DTRACE_IOCTL_H
#define	_SYS_DTRACE_IOCTL_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/ioctl.h>

/*
 * DTrace Pseudodevice Interface
 *
 * DTrace is controlled through ioctl(2)'s to the in-kernel dtrace:dtrace
 * pseudodevice driver.  These ioctls comprise the user-kernel interface to
 * DTrace.
 */
#define	DTRACEIOC		0xd4
#define	DTRACEIOC_PROVIDER	_IOR(DTRACEIOC, 1, dtrace_providerdesc_t)	/* provider query */
#define	DTRACEIOC_PROBES	_IOR(DTRACEIOC, 2, dtrace_probedesc_t)	/* probe query */
#define	DTRACEIOC_BUFSNAP	_IOR(DTRACEIOC, 4, dtrace_bufdesc_t)	/* snapshot buffer */
#define	DTRACEIOC_PROBEMATCH	_IOR(DTRACEIOC, 5, dtrace_probedesc_t)	/* match probes */
#define	DTRACEIOC_ENABLE	_IOW(DTRACEIOC, 6, void *)	/* enable probes */
#define	DTRACEIOC_AGGSNAP	_IOR(DTRACEIOC, 7, dtrace_bufdesc_t)	/* snapshot agg. */
#define	DTRACEIOC_EPROBE	_IOW(DTRACEIOC, 8, dtrace_eprobedesc_t)	/* get eprobe desc. */
#define	DTRACEIOC_PROBEARG	_IOR(DTRACEIOC, 9, dtrace_argdesc_t)	/* get probe arg */
#define	DTRACEIOC_CONF		_IOR(DTRACEIOC, 10, dtrace_conf_t)	/* get config. */
#define	DTRACEIOC_STATUS	_IOR(DTRACEIOC, 11, dtrace_status_t)	/* get status */
#define	DTRACEIOC_GO		_IOW(DTRACEIOC, 12, processorid_t)	/* start tracing */
#define	DTRACEIOC_STOP		_IOW(DTRACEIOC, 13, processorid_t)	/* stop tracing */
#define	DTRACEIOC_AGGDESC	_IOR(DTRACEIOC, 15, dtrace_aggdesc_t)	/* get agg. desc. */
#define	DTRACEIOC_FORMAT	_IOR(DTRACEIOC, 16, dtrace_fmtdesc_t)	/* get format str */
#define	DTRACEIOC_DOFGET	_IOR(DTRACEIOC, 17, dof_hdr_t)	/* get DOF */
#define	DTRACEIOC_REPLICATE	_IOR(DTRACEIOC, 18, void *)	/* replicate enab */

/*
 * DTrace Helpers
 *
 * In general, DTrace establishes probes in processes and takes actions on
 * processes without knowing their specific user-level structures.  Instead of
 * existing in the framework, process-specific knowledge is contained by the
 * enabling D program -- which can apply process-specific knowledge by making
 * appropriate use of DTrace primitives like copyin() and copyinstr() to
 * operate on user-level data.  However, there may exist some specific probes
 * of particular semantic relevance that the application developer may wish to
 * explicitly export.  For example, an application may wish to export a probe
 * at the point that it begins and ends certain well-defined transactions.  In
 * addition to providing probes, programs may wish to offer assistance for
 * certain actions.  For example, in highly dynamic environments (e.g., Java),
 * it may be difficult to obtain a stack trace in terms of meaningful symbol
 * names (the translation from instruction addresses to corresponding symbol
 * names may only be possible in situ); these environments may wish to define
 * a series of actions to be applied in situ to obtain a meaningful stack
 * trace.
 *
 * These two mechanisms -- user-level statically defined tracing and assisting
 * DTrace actions -- are provided via DTrace _helpers_.  Helpers are specified
 * via DOF, but unlike enabling DOF, helper DOF may contain definitions of
 * providers, probes and their arguments.  If a helper wishes to provide
 * action assistance, probe descriptions and corresponding DIF actions may be
 * specified in the helper DOF.  For such helper actions, however, the probe
 * description describes the specific helper:  all DTrace helpers have the
 * provider name "dtrace" and the module name "helper", and the name of the
 * helper is contained in the function name (for example, the ustack() helper
 * is named "ustack").  Any helper-specific name may be contained in the name
 * (for example, if a helper were to have a constructor, it might be named
 * "dtrace:helper:<helper>:init").  Helper actions are only called when the
 * action that they are helping is taken.  Helper actions may only return DIF
 * expressions, and may only call the following subroutines:
 *
 *    alloca()      <= Allocates memory out of the consumer's scratch space
 *    bcopy()       <= Copies memory to scratch space
 *    copyin()      <= Copies memory from user-level into consumer's scratch
 *    copyinto()    <= Copies memory into a specific location in scratch
 *    copyinstr()   <= Copies a string into a specific location in scratch
 *
 * Helper actions may only access the following built-in variables:
 *
 *    curthread     <= Current kthread_t pointer
 *    tid           <= Current thread identifier
 *    pid           <= Current process identifier
 *    ppid          <= Parent process identifier
 *    uid           <= Current user ID
 *    gid           <= Current group ID
 *    execname      <= Current executable name
 *    zonename      <= Current zone name
 *
 * Helper actions may not manipulate or allocate dynamic variables, but they
 * may have clause-local and statically-allocated global variables.  The
 * helper action variable state is specific to the helper action -- variables
 * used by the helper action may not be accessed outside of the helper
 * action, and the helper action may not access variables that like outside
 * of it.  Helper actions may not load from kernel memory at-large; they are
 * restricting to loading current user state (via copyin() and variants) and
 * scratch space.  As with probe enablings, helper actions are executed in
 * program order.  The result of the helper action is the result of the last
 * executing helper expression.
 *
 * Helpers -- composed of either providers/probes or probes/actions (or both)
 * -- are added by opening the "helper" minor node, and issuing an ioctl(2)
 * (DTRACEHIOC_ADDDOF) that specifies the dof_helper_t structure. This
 * encapsulates the name and base address of the user-level library or
 * executable publishing the helpers and probes as well as the DOF that
 * contains the definitions of those helpers and probes.
 *
 * The DTRACEHIOC_ADD and DTRACEHIOC_REMOVE are left in place for legacy
 * helpers and should no longer be used.  No other ioctls are valid on the
 * helper minor node.
 */
#define	DTRACEHIOC		0xd8
#define	DTRACEHIOC_ADD		_IOW(DTRACEHIOC, 1, int)	/* add helper */
#define	DTRACEHIOC_REMOVE	_IOW(DTRACEHIOC, 2, int)	/* remove helper */
#define	DTRACEHIOC_ADDDOF	_IOW(DTRACEHIOC, 3, dof_helper_t)	/* add helper DOF */

typedef struct dof_helper {
	char dofhp_mod[DTRACE_MODNAMELEN];	/* executable or library name */
	uint64_t dofhp_addr;			/* base address of object */
	uint64_t dofhp_dof;			/* address of helper DOF */
} dof_helper_t;

#endif	/* __ASSEMBLY__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DTRACE_IOCTL_H */
