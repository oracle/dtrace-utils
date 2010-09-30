/*
 * FILE:	dtrace_dif.c
 * DESCRIPTION:	Dynamic Tracing: DIF object functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/slab.h>

#include "dtrace.h"

static uint64_t	dtrace_vtime_references;

void dtrace_difo_hold(dtrace_difo_t *dp)
{
	int	i;

	dp->dtdo_refcnt++;
	ASSERT(dp->dtdo_refcnt != 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t	*v = &dp->dtdo_vartab[i];

		if (v->dtdv_id != DIF_VAR_VTIMESTAMP)
			continue;

		if (dtrace_vtime_references++ == 0)
			dtrace_vtime_enable();
	}
}

void dtrace_difo_destroy(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	int	i;

	ASSERT(dp->dtdo_refcnt == 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t		*v = &dp->dtdo_vartab[i];
		dtrace_statvar_t	*svar, **svarp;
		uint_t			id;
		uint8_t			scope = v->dtdv_scope;
		int			*np;

		switch (scope) {
		case DIFV_SCOPE_THREAD:
			continue;

		case DIFV_SCOPE_LOCAL:
			np = &vstate->dtvs_nlocals;
			svarp = vstate->dtvs_locals;
			break;

		case DIFV_SCOPE_GLOBAL:
			np = &vstate->dtvs_nglobals;
			svarp = vstate->dtvs_globals;
			break;

		default:
			BUG();
		}

		if ((id = v->dtdv_id) < DIF_VAR_OTHER_UBASE)
			continue;

		id -= DIF_VAR_OTHER_UBASE;
		ASSERT(id < *np);

		svar = svarp[id];
		ASSERT(svar != NULL);
		ASSERT(svar->dtsv_refcnt > 0);

		if (--svar->dtsv_refcnt > 0)
			continue;

		if (svar->dtsv_size != 0) {
			ASSERT((void *)(uintptr_t)svar->dtsv_data != NULL);
			kfree((void *)(uintptr_t)svar->dtsv_data);
		}

		kfree(svar);
		svarp[id] = NULL;
	}

	kfree(dp->dtdo_buf);
        kfree(dp->dtdo_inttab);
        kfree(dp->dtdo_strtab);
        kfree(dp->dtdo_vartab);
        kfree(dp);
}

void dtrace_difo_release(dtrace_difo_t *dp, dtrace_vstate_t *vstate)
{
	int	i;

	ASSERT(mutex_is_locked(&dtrace_lock));
	ASSERT(dp->dtdo_refcnt != 0);

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t *v = &dp->dtdo_vartab[i];

		if (v->dtdv_id != DIF_VAR_VTIMESTAMP)
			continue;

		ASSERT(dtrace_vtime_references > 0);

		if (--dtrace_vtime_references == 0)
			dtrace_vtime_disable();
	}

	if (--dp->dtdo_refcnt == 0)
		dtrace_difo_destroy(dp, vstate);
}
