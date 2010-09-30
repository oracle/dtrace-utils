/*
 * FILE:	dtrace_state.c
 * DESCRIPTION:	Dynamic Tracing: consumer state functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/slab.h>

#include "dtrace.h"

struct kmem_cache	*dtrace_state_cache;
dtrace_optval_t		dtrace_nspec_default = 1;
dtrace_optval_t		dtrace_specsize_default = 32 * 1024;
size_t			dtrace_strsize_default = 256;
dtrace_optval_t		dtrace_stackframes_default = 20;
dtrace_optval_t		dtrace_ustackframes_default = 20;
dtrace_optval_t		dtrace_cleanrate_default = 9900990;
dtrace_optval_t		dtrace_aggrate_default = NANOSEC;
dtrace_optval_t		dtrace_switchrate_default = NANOSEC;
dtrace_optval_t		dtrace_statusrate_default = NANOSEC;
dtrace_optval_t		dtrace_jstackframes_default = 50;
dtrace_optval_t		dtrace_jstackstrsize_default = 512;

void dtrace_dstate_fini(dtrace_dstate_t *dstate)
{
	/* FIXME: ASSERT(mutex_is_locked(&cpu_lock)); */

	if (dstate->dtds_base == NULL)
		return;

	kfree(dstate->dtds_base);
	kmem_cache_free(dtrace_state_cache, dstate->dtds_percpu); /* FIXME */
}

void dtrace_vstate_fini(dtrace_vstate_t *vstate)
{
	/*
	 * If only there was a logical XOR operator...
	 */
	ASSERT((vstate->dtvs_nglobals == 0) ^ (vstate->dtvs_globals != NULL));

	if (vstate->dtvs_nglobals > 0)
		kfree(vstate->dtvs_globals);

	if (vstate->dtvs_ntlocals > 0)
		kfree(vstate->dtvs_tlocals);

	ASSERT((vstate->dtvs_nlocals == 0) ^ (vstate->dtvs_locals != NULL));

	if (vstate->dtvs_nlocals > 0)
		kfree(vstate->dtvs_locals);
}

dtrace_state_t *dtrace_state_create(struct file *file)
{
	dtrace_state_t	*state;
	dtrace_optval_t	*opt;
	int		bufsize = NR_CPUS * sizeof (dtrace_buffer_t), i;
	const cred_t	*cr = file->f_cred;

	ASSERT(mutex_is_locked(&dtrace_lock));
	/* FIXME: ASSERT(mutex_is_locked(&cpu_lock)); */

	state = kmalloc(sizeof (dtrace_state_t), GFP_KERNEL);
	state->dts_epid = DTRACE_EPIDNONE + 1;
	/* state->dts_dev = NULL;  -- FIXME: Do we even need this? */
	state->dts_buffer = kzalloc(bufsize, GFP_KERNEL);
	state->dts_aggbuffer = kzalloc(bufsize, GFP_KERNEL);
	state->dts_cleaner = 0;
	state->dts_deadman = 0;
	state->dts_vstate.dtvs_state = state;

	for (i = 0; i < DTRACEOPT_MAX; i++)
		state->dts_options[i] = DTRACEOPT_UNSET;

	/*
	 * Set the default options.
	 */
	opt = state->dts_options;
	opt[DTRACEOPT_BUFPOLICY] = DTRACEOPT_BUFPOLICY_SWITCH;
	opt[DTRACEOPT_BUFRESIZE] = DTRACEOPT_BUFRESIZE_AUTO;
	opt[DTRACEOPT_NSPEC] = dtrace_nspec_default;
	opt[DTRACEOPT_SPECSIZE] = dtrace_specsize_default;
	opt[DTRACEOPT_CPU] = (dtrace_optval_t)DTRACE_CPUALL;
	opt[DTRACEOPT_STRSIZE] = dtrace_strsize_default;
	opt[DTRACEOPT_STACKFRAMES] = dtrace_stackframes_default;
	opt[DTRACEOPT_USTACKFRAMES] = dtrace_ustackframes_default;
	opt[DTRACEOPT_CLEANRATE] = dtrace_cleanrate_default;
	opt[DTRACEOPT_AGGRATE] = dtrace_aggrate_default;
	opt[DTRACEOPT_SWITCHRATE] = dtrace_switchrate_default;
	opt[DTRACEOPT_STATUSRATE] = dtrace_statusrate_default;
	opt[DTRACEOPT_JSTACKFRAMES] = dtrace_jstackframes_default;
	opt[DTRACEOPT_JSTACKSTRSIZE] = dtrace_jstackstrsize_default;

	state->dts_activity = DTRACE_ACTIVITY_INACTIVE;

#ifdef FIXME
	/*
	 * Set probe visibility and destructiveness based on user credential
	 * information.  For actual anonymous tracing or if all privileges are
	 * set, checks are bypassed.
	 */
	if (cr == NULL ||
	    PRIV_POLICY_ONLY(cr, PRIV_ALL, FALSE)) {
		state->dts_cred.dcr_visible = DTRACE_CRV_ALL;
		state->dts_cred.dcr_action = DTRACE_CRA_ALL;
	} else {
		state->dts_cred.dcr_cred = get_cred(cr);

		/*
		 * CRA_PROC means "we have *some* privilege for dtrace" and
		 * it unlocks the use of variables like pid, etc.
		 */
		if (PRIV_POLICY_ONLY(cr, PRIV_DTRACE_USER, FALSE) ||
		    PRIV_POLICY_ONLY(cr, PRIV_DTRACE_PROC, FALSE))
			state->dts_cred.dcr_action |= DTRACE_CRA_PROC;

		/*
		 * The DTRACE_USER privilege allows the use of syscall and
		 * profile providers.  If the user also has PROC_OWNER, we
		 * extend the scope to include additional visibility and
		 * destructive power.
		 */
		if (PRIV_POLICY_ONLY(cr, PRIV_DTRACE_USER, FALSE)) {
			if (PRIV_POLICY_ONLY(cr, PRIV_PROC_OWNER, FALSE))
				state->dts_cred.dcr_visible |=
					DTRACE_CRV_ALLPROC;

			state->dts_cred.dcr_action |=
					DTRACE_CRA_PROC_DESTRUCTIVE_ALLUSER;
		}

		/*
		 * Holding the DTRACE_KERNEL privilege also implies that
		 * the user has the DTRACE_USER privilege from a visibility
		 * perspective.  But without further privileges, some
		 * destructive actions are not available.
		 */
		if (PRIV_POLICY_ONLY(cr, PRIV_DTRACE_KERNEL, FALSE)) {
			/*
			 * Make all probes in all zones visible.  However,
			 * this doesn't mean that all actions become available
			 * to all zones.
			 */
			state->dts_cred.dcr_visible |= DTRACE_CRV_KERNEL |
						       DTRACE_CRV_ALLPROC;
			state->dts_cred.dcr_action |= DTRACE_CRA_KERNEL |
						      DTRACE_CRA_PROC;

			/*
			 * Holding PROC_OWNER means that destructive actions
			 * are allowed.
			 */
			if (PRIV_POLICY_ONLY(cr, PRIV_PROC_OWNER, FALSE))
				state->dts_cred.dcr_action |=
					DTRACE_CRA_PROC_DESTRUCTIVE_ALLUSER;
		}

		/*
		 * Holding the DTRACE_PROC privilege gives control over the
		 * fasttrap and pid providers.  We need to grant wider
		 * destructive privileges in the event that the user has
		 * PROC_OWNER .
		*/
		if (PRIV_POLICY_ONLY(cr, PRIV_DTRACE_PROC, FALSE)) {
			if (PRIV_POLICY_ONLY(cr, PRIV_PROC_OWNER, FALSE))
				state->dts_cred.dcr_action |=
					DTRACE_CRA_PROC_DESTRUCTIVE_ALLUSER;
		}
	}
#endif

	return state;
}

void dtrace_state_destroy(dtrace_state_t *state)
{
	dtrace_ecb_t		*ecb;
	dtrace_vstate_t		*vstate = &state->dts_vstate;
	int			i;
	dtrace_speculation_t	*spec = state->dts_speculations;
	int			nspec = state->dts_nspeculations;
	uint32_t		match;

	ASSERT(mutex_is_locked(&dtrace_lock));
	/* FIXME: ASSERT(mutex_is_locked(&cpu_lock)); */

	/*
	 * First, retract any retained enablings for this state.
	 */
	dtrace_enabling_retract(state);
	ASSERT(state->dts_nretained == 0);

	if (state->dts_activity == DTRACE_ACTIVITY_ACTIVE ||
	    state->dts_activity == DTRACE_ACTIVITY_DRAINING) {
		/*
		 * We have managed to come into dtrace_state_destroy() on a
		 * hot enabling -- almost certainly because of a disorderly
		 * shutdown of a consumer.  (That is, a consumer that is
		 * exiting without having called dtrace_stop().) In this case,
		 * we're going to set our activity to be KILLED, and then
		 * issue a sync to be sure that everyone is out of probe
		 * context before we start blowing away ECBs.
		 */
		state->dts_activity = DTRACE_ACTIVITY_KILLED;
		dtrace_sync();
	}

	/*
	 * Release the credential hold we took in dtrace_state_create().
	 */
	if (state->dts_cred.dcr_cred != NULL)
		put_cred(state->dts_cred.dcr_cred);

	/*
	 * Now we can safely disable and destroy any enabled probes.  Because
	 * any DTRACE_PRIV_KERNEL probes may actually be slowing our progress
	 * (especially if they're all enabled), we take two passes through the
	 * ECBs: in the first, we disable just DTRACE_PRIV_KERNEL probes, and
	 * in the second we disable whatever is left over.
	*/
	for (match = DTRACE_PRIV_KERNEL; ; match = 0) {
		for (i = 0; i < state->dts_necbs; i++) {
			if ((ecb = state->dts_ecbs[i]) == NULL)
				continue;

			if (match && ecb->dte_probe != NULL) {
				dtrace_probe_t		*probe =
							ecb->dte_probe;
				dtrace_provider_t	*prov =
							probe->dtpr_provider;

				if (!(prov->dtpv_priv.dtpp_flags & match))
					continue;
			}

			dtrace_ecb_disable(ecb);
			dtrace_ecb_destroy(ecb);
		}

		if (!match)
			break;
	}

	/*
	 * Before we free the buffers, perform one more sync to assure that
	 * every CPU is out of probe context.
	 */
	dtrace_sync();

	dtrace_buffer_free(state->dts_buffer);
	dtrace_buffer_free(state->dts_aggbuffer);

	for (i = 0; i < nspec; i++)
		dtrace_buffer_free(spec[i].dtsp_buffer);

	if (state->dts_cleaner != CYCLIC_NONE)
		cyclic_remove(state->dts_cleaner);

	if (state->dts_deadman != CYCLIC_NONE)
		cyclic_remove(state->dts_deadman);

	dtrace_dstate_fini(&vstate->dtvs_dynvars);
	dtrace_vstate_fini(vstate);
	kfree(state->dts_ecbs);

	if (state->dts_aggregations != NULL) {
#ifdef DEBUG
		for (i = 0; i < state->dts_naggregations; i++)
			ASSERT(state->dts_aggregations[i] == NULL);
#endif

		ASSERT(state->dts_naggregations > 0);
		kfree(state->dts_aggregations);
	}

	kfree(state->dts_buffer);
	kfree(state->dts_aggbuffer);

	for (i = 0; i < nspec; i++)
		kfree(spec[i].dtsp_buffer);

	kfree(spec);

	dtrace_format_destroy(state);
}
