/*
 * FILE:	dtrace_enable.c
 * DESCRIPTION:	Dynamic Tracing: enabling functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/kernel.h>
#include <linux/mutex.h>

#include "dtrace.h"

dtrace_enabling_t	*dtrace_retained;
dtrace_genid_t		dtrace_retained_gen;

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
