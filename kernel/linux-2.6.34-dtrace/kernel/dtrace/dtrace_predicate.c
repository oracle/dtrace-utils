/*
 * FILE:	dtrace_predicate.c
 * DESCRIPTION:	Dynamic Tracing: predicate functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/slab.h>

#include "dtrace.h"

void dtrace_predicate_hold(dtrace_predicate_t *pred)
{
	BUG_ON(pred->dtp_difo == NULL || pred->dtp_difo->dtdo_refcnt == 0);
	BUG_ON(pred->dtp_refcnt <= 0);

	pred->dtp_refcnt++;
}

void dtrace_predicate_release(dtrace_predicate_t *pred,
			      dtrace_vstate_t *vstate)
{
	dtrace_difo_t *dp = pred->dtp_difo;

	BUG_ON(dp == NULL || dp->dtdo_refcnt == 0);
	BUG_ON(pred->dtp_refcnt <= 0);

	if (--pred->dtp_refcnt == 0) {
		dtrace_difo_release(pred->dtp_difo, vstate);
		kfree(pred);
	}
}
