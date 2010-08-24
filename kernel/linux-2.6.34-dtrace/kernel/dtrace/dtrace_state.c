/*
 * FILE:	dtrace_state.c
 * DESCRIPTION:	Dynamic Tracing: consumer state functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/slab.h>

#include "dtrace.h"

dtrace_optval_t	dtrace_nspec_default = 1;
dtrace_optval_t	dtrace_specsize_default = 32 * 1024;
size_t		dtrace_strsize_default = 256;
dtrace_optval_t	dtrace_stackframes_default = 20;
dtrace_optval_t	dtrace_ustackframes_default = 20;
dtrace_optval_t	dtrace_cleanrate_default = 9900990;
dtrace_optval_t	dtrace_aggrate_default = NANOSEC;
dtrace_optval_t	dtrace_switchrate_default = NANOSEC;
dtrace_optval_t	dtrace_statusrate_default = NANOSEC;
dtrace_optval_t	dtrace_jstackframes_default = 50;
dtrace_optval_t	dtrace_jstackstrsize_default = 512;

dtrace_state_t *dtrace_state_create(struct file *file)
{
	dtrace_state_t	*state;
	dtrace_optval_t	*opt;
	int		bufsize = NR_CPUS * sizeof (dtrace_buffer_t), i;
	const cred_t	*cr = file->f_cred;

	BUG_ON(!mutex_is_locked(&dtrace_lock));
	/* FIXME: BUG_ON(!mutex_is_locked(&cpu_lock)); */

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
	 * Set robe visibility and destructiveness based on the user credential
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
