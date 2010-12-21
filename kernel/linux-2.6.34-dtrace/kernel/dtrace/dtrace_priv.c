/*
 * FILE:	dtrace_priv.c
 * DESCRIPTION:	Dynamic Tracing: privilege check functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include "dtrace.h"

int dtrace_priv_proc_destructive(dtrace_state_t *state)
{
	int	action = state->dts_cred.dcr_action;

	if (((action & DTRACE_CRA_PROC_DESTRUCTIVE_ALLZONE) == 0) &&
	    dtrace_priv_proc_common_zone(state) == 0)
		goto bad;

	if (((action & DTRACE_CRA_PROC_DESTRUCTIVE_ALLUSER) == 0) &&
	    dtrace_priv_proc_common_user(state) == 0)
		goto bad;

	if (((action & DTRACE_CRA_PROC_DESTRUCTIVE_CREDCHG) == 0) &&
	    dtrace_priv_proc_common_nocd() == 0)
		goto bad;

	return 1;

bad:
	cpu_core[smp_processor_id()].cpuc_dtrace_flags |= CPU_DTRACE_UPRIV;
   
	return 0;
}

int dtrace_priv_proc_control(dtrace_state_t *state)
{
	if (state->dts_cred.dcr_action & DTRACE_CRA_PROC_CONTROL)
		return 1;

	if (dtrace_priv_proc_common_zone(state) &&
	    dtrace_priv_proc_common_user(state) &&
	    dtrace_priv_proc_common_nocd())
		return 1;

	cpu_core[smp_processor_id()].cpuc_dtrace_flags |= CPU_DTRACE_UPRIV;

	return 0;
}

int dtrace_priv_proc(dtrace_state_t *state)
{
	if (state->dts_cred.dcr_action & DTRACE_CRA_PROC)
		return 1;

	cpu_core[smp_processor_id()].cpuc_dtrace_flags |= CPU_DTRACE_UPRIV;

	return 0;
}

int dtrace_priv_kernel(dtrace_state_t *state)
{
	if (state->dts_cred.dcr_action & DTRACE_CRA_KERNEL)
		return 1;

	cpu_core[smp_processir_id()].cpuc_dtrace_flags |= CPU_DTRACE_KPRIV;

	return 0;
}
