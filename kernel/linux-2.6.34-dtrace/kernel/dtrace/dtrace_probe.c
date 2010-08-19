/*
 * FILE:	dtrace_probe.c
 * DESCRIPTION:	Dynamic Tracing: probe management functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>

#include "dtrace.h"

int		dtrace_nprobes;
dtrace_probe_t	**dtrace_probes;

int dtrace_probe_enable(const dtrace_probedesc_t *desc, dtrace_enabling_t *enab)
{
	dtrace_probekey_t	pkey;
	uint32_t		priv;
	uid_t			uid;

	dtrace_ecb_create_cache = NULL;

	if (desc == NULL) {
		(void) dtrace_ecb_create_enable(NULL, enab);

		return 0;
	}

	dtrace_probekey(desc, &pkey);
	dtrace_cred2priv(enab->dten_vstate->dtvs_state->dts_cred.dcr_cred,
			 &priv, &uid);

	return dtrace_match(&pkey, priv, uid, dtrace_ecb_create_enable, enab);
}

void dtrace_probe_provide(dtrace_probedesc_t *desc, dtrace_provider_t *prv)
{
	struct module	*mod;
	int		all = 0;

	if (prv == NULL) {
		all = 1;
		prv = dtrace_provider;
	}

	do {
		prv->dtpv_pops.dtps_provide(prv->dtpv_arg, desc);

#ifdef FIXME
/*
 * This needs work because (so far) I have not found a way to get access to the
 * list of modules in Linux.
 */
		mutex_lock(&module_mutex);

		list_for_each_entry(mod, &modules, list) {
			if (mod->state != MODULE_STATE_LIVE)
				continue;

			prv->dtpv_pops.dtps_provide_module(prv->dtpv_arg, mod);
		}

		mutex_unlock(&module_mutex);
#endif
	} while (all && (prv = prv->dtpv_next) != NULL);
}

dtrace_probe_t *dtrace_probe_lookup_id(dtrace_id_t id)
{
	if (id == 0 || id > dtrace_nprobes)
		return NULL;

	return dtrace_probes[id - 1];
}
