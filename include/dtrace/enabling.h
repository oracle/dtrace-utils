/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2024, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_ENABLING_H
#define _DTRACE_ENABLING_H

#include <dtrace/universal.h>
#include <dtrace/difo_defines.h>

/*
 * FIXME: Needs to be rewritten.
 *
 * When DTrace is tracking the description of a DTrace enabling entity (probe,
 * predicate, action, ECB, record, etc.), it does so in a description
 * structure.  These structures all end in "desc", and are used at both
 * user-level and in the kernel -- but (with the exception of
 * dtrace_probedesc_t) they are never passed between them.  Typically,
 * user-level will use the description structures when assembling an enabling.
 * It will then distill those description structures into a DOF object (see
 * above), and send it into the kernel.  The kernel will again use the
 * description structures to create a description of the enabling as it reads
 * the DOF.  When the description is complete, the enabling will be actually
 * created -- turning it into the structures that represent the enabling
 * instead of merely describing it.  Not surprisingly, the description
 * structures bear a strong resemblance to the DOF structures that act as their
 * conduit.
 */

typedef struct dtrace_probedesc {
	dtrace_id_t	id;			/* probe identifier */
	const char	*prv;			/* probe provider name */
	const char	*mod;			/* probe module name */
	const char	*fun;			/* probe function name */
	const char	*prb;			/* probe name */
} dtrace_probedesc_t;

typedef struct dtrace_repldesc {
	dtrace_probedesc_t dtrpd_match;		/* probe descr. to match */
	dtrace_probedesc_t dtrpd_create;	/* probe descr. to create */
} dtrace_repldesc_t;

typedef struct dtrace_actdesc {
	struct dtrace_difo *dtad_difo;		/* pointer to DIF object */
	dtrace_actkind_t dtad_kind;		/* kind of action */
	uint32_t dtad_ntuple;			/* number in tuple */
	uint64_t dtad_arg;			/* action argument */
	uint64_t dtad_uarg;			/* user argument */
} dtrace_actdesc_t;

typedef struct dtrace_ecbdesc {
	dtrace_probedesc_t dted_probe;		/* probe description */
	uint64_t dted_uarg;			/* library argument */
	int dted_refcnt;			/* reference count */
} dtrace_ecbdesc_t;

#endif /* _DTRACE_ENABLING_H */
