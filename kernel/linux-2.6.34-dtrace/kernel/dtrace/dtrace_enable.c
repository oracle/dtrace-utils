/*
 * FILE:	dtrace_enable.c
 * DESCRIPTION:	Dynamic Tracing: enabling functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "dtrace.h"

dtrace_enabling_t	*dtrace_retained;
dtrace_genid_t		dtrace_retained_gen;

void dtrace_enabling_destroy(dtrace_enabling_t *enab)
{
	int			i;
	dtrace_ecbdesc_t	*ep;
	dtrace_vstate_t		*vstate = enab->dten_vstate;

	ASSERT(mutex_is_locked(&dtrace_lock));

	for (i = 0; i < enab->dten_ndesc; i++) {
		dtrace_actdesc_t	*act, *next;
		dtrace_predicate_t	*pred;

		ep = enab->dten_desc[i];

		if ((pred = ep->dted_pred.dtpdd_predicate) != NULL)
			dtrace_predicate_release(pred, vstate);

		for (act = ep->dted_action; act != NULL; act = next) {
			next = act->dtad_next;
			dtrace_actdesc_release(act, vstate);
		}

		kfree(ep);
	}

	kfree(enab->dten_desc);

	/*
	 * If this was a retained enabling, decrement the dts_nretained count
	 * and remove it from the dtrace_retained list.
	 */
	if (enab->dten_prev != NULL || enab->dten_next != NULL ||
	    dtrace_retained == enab) {
		ASSERT(enab->dten_vstate->dtvs_state != NULL);
		ASSERT(enab->dten_vstate->dtvs_state->dts_nretained > 0);
		enab->dten_vstate->dtvs_state->dts_nretained--;
		dtrace_retained_gen++;
	}

	if (enab->dten_prev == NULL) {
		if (dtrace_retained == enab) {
			dtrace_retained = enab->dten_next;

			if (dtrace_retained != NULL)
				dtrace_retained->dten_prev = NULL;
		}
	} else {
		ASSERT(enab != dtrace_retained);
		ASSERT(dtrace_retained != NULL);
		enab->dten_prev->dten_next = enab->dten_next;
	}

	if (enab->dten_next != NULL) {
		ASSERT(dtrace_retained != NULL);
		enab->dten_next->dten_prev = enab->dten_prev;
	}

	kfree(enab);
}

void dtrace_enabling_retract(dtrace_state_t *state)
{
	dtrace_enabling_t	*enab, *next;

	ASSERT(mutex_is_locked(&dtrace_lock));

	/*
	 * Iterate over all retained enablings, destroy the enablings retained
	 * for the specified state.
	 */
	for (enab = dtrace_retained; enab != NULL; enab = next) {
		next = enab->dten_next;

		/*
		 * dtvs_state can only be NULL for helper enablings, and helper
		 * enablings can't be retained.
		 */
		ASSERT(enab->dten_vstate->dtvs_state != NULL);

		if (enab->dten_vstate->dtvs_state == state) {
			ASSERT(state->dts_nretained > 0);
			dtrace_enabling_destroy(enab);
		}
	}

	ASSERT(state->dts_nretained == 0);
}

int dtrace_enabling_match(dtrace_enabling_t *enab, int *nmatched)
{
	int	i;
	int	total_matched = 0, matched = 0;

	for (i = 0; i < enab->dten_ndesc; i++) {
		dtrace_ecbdesc_t	*ep = enab->dten_desc[i];

		enab->dten_current = ep;
		enab->dten_error = 0;

		if ((matched = dtrace_probe_enable(&ep->dted_probe, enab)) < 0)
			return -EBUSY;

		total_matched += matched;

		if (enab->dten_error != 0) {
			if (nmatched == NULL)
				pr_warning("dtrace_enabling_match() error on %p: %d\n", (void *)ep, enab->dten_error);

			return enab->dten_error;
		}
	}

	enab->dten_probegen = dtrace_probegen;
	if (nmatched != NULL)
		*nmatched = total_matched;

	return 0;
}

void dtrace_enabling_matchall(void)
{
	dtrace_enabling_t	*enab;

#ifdef FIXME
	mutex_lock(&cpu_lock);
#endif
	mutex_lock(&dtrace_lock);

	for (enab = dtrace_retained; enab != NULL; enab = enab->dten_next)
		(void) dtrace_enabling_match(enab, NULL);

	mutex_unlock(&dtrace_lock);
#ifdef FIXME
	mutex_unlock(&cpu_lock);
#endif
}

void dtrace_enabling_provide(dtrace_provider_t *prv)
{
	int		all = 0;
	dtrace_genid_t	gen;

	if (prv == NULL) {
		all = 1;
		prv = dtrace_provider;
	}

	do {
		dtrace_enabling_t	*enab;
		void			*parg = prv->dtpv_arg;

retry:
		gen = dtrace_retained_gen;
		for (enab = dtrace_retained; enab != NULL;
		     enab = enab->dten_next) {
			int	i;

			for (i = 0; i < enab->dten_ndesc; i++) {
				dtrace_probedesc_t	desc;

				desc = enab->dten_desc[i]->dted_probe;
				mutex_unlock(&dtrace_lock);
				prv->dtpv_pops.dtps_provide(parg, &desc);
				mutex_lock(&dtrace_lock);

				if (gen != dtrace_retained_gen)
					goto retry;
			}
		}
	} while (all && (prv = prv->dtpv_next) != NULL);

	mutex_unlock(&dtrace_lock);
	dtrace_probe_provide(NULL, all ? NULL : prv);
	mutex_lock(&dtrace_lock);
}
