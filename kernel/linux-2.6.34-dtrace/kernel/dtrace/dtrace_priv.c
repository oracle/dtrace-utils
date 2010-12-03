/*
 * FILE:	dtrace_priv.c
 * DESCRIPTION:	Dynamic Tracing: privilege check functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include "dtrace.h"

int dtrace_priv_kernel(dtrace_state_t *state)
{
	if (state->dts_cred.dcr_action & DTRACE_CRA_KERNEL)
		return 1;

	cpu_core[CPU->cpu_id].cpuc_dtrace_flags |= CPU_DTRACE_KPRIV;

	return 0;
}

int dtrace_priv_proc(dtrace_state_t *state)
{
	if (state->dts_cred.dcr_action & DTRACE_CRA_PROC)
		return 1;

	cpu_core[CPU->cpu_id].cpuc_dtrace_flags |= CPU_DTRACE_UPRIV;

	return 0;
}
