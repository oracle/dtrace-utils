/*
 * FILE:	dtrace_actdesc.c
 * DESCRIPTION:	Dynamic Tracing: action description functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/slab.h>

#include "dtrace.h"

dtrace_actdesc_t *dtrace_actdesc_create(dtrace_actkind_t kind, uint32_t ntuple,
					uint64_t uarg, uint64_t arg)
{
	dtrace_actdesc_t	*act;

	ASSERT(!DTRACEACT_ISPRINTFLIKE(kind) ||
	       (arg != NULL && arg >= KERNELBASE) ||
	       (arg == NULL && kind == DTRACEACT_PRINTA));

	act = kzalloc(sizeof (dtrace_actdesc_t), GFP_KERNEL);
	act->dtad_kind = kind;
	act->dtad_ntuple = ntuple;
	act->dtad_uarg = uarg;
	act->dtad_arg = arg;
	act->dtad_refcnt = 1;

	return act;
}

void dtrace_actdesc_hold(dtrace_actdesc_t *act)
{
	ASSERT(act->dtad_refcnt >= 1);

	act->dtad_refcnt++;
}

void dtrace_actdesc_release(dtrace_actdesc_t *act, dtrace_vstate_t *vstate)
{
	dtrace_actkind_t	kind = act->dtad_kind;
	dtrace_difo_t		*dp;

	ASSERT(act->dtad_refcnt >= 1);

	if (--act->dtad_refcnt != 0)
		return;

	if ((dp = act->dtad_difo) != NULL)
		dtrace_difo_release(dp, vstate);

	if (DTRACEACT_ISPRINTFLIKE(kind)) {
		char	*str = (char *)(uintptr_t)act->dtad_arg;

		ASSERT((str != NULL && (uintptr_t)str >= KERNELBASE) ||
		       (str == NULL && act->dtad_kind == DTRACEACT_PRINTA));

		if (str != NULL)
			kfree(str);
	}

	kfree(act);
}
