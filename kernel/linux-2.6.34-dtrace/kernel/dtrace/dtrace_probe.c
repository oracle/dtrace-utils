/*
 * FILE:	dtrace_probe.c
 * DESCRIPTION:	Dynamic Tracing: probe management functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/idr.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "dtrace.h"

struct idr	dtrace_probe_idr;

/*
 * Create a new probe.
 */
dtrace_id_t dtrace_probe_create(dtrace_provider_id_t prov, const char *mod,
				const char *func, const char *name,
				int aframes, void *arg)
{
	dtrace_probe_t		*probe;
	dtrace_provider_t	*provider = (dtrace_provider_t *)prov;
	dtrace_id_t		id;
	int			err;

	probe = kzalloc(sizeof (dtrace_probe_t), __GFP_NOFAIL);

	/*
	 * The idr_pre_get() function should be called without holding locks.
	 * When the provider is the DTrace core itself, dtrace_lock will be
	 * held when we enter this function.
	 */
	if (provider == dtrace_provider) {
		ASSERT(mutex_is_locked(&dtrace_lock));
		mutex_unlock(&dtrace_lock);
	}

again:
	idr_pre_get(&dtrace_probe_idr, __GFP_NOFAIL);

	mutex_lock(&dtrace_lock);
	err = idr_get_new(&dtrace_probe_idr, probe, &id);
	if (err == -EAGAIN) {
		mutex_unlock(&dtrace_lock);
		goto again;
	}

	probe->dtpr_id = id;
	probe->dtpr_gen = dtrace_probegen++;
	probe->dtpr_mod = kstrdup(mod, GFP_KERNEL);
	probe->dtpr_func = kstrdup(func, GFP_KERNEL);
	probe->dtpr_name = kstrdup(name, GFP_KERNEL);
	probe->dtpr_arg = arg;
	probe->dtpr_aframes = aframes;
	probe->dtpr_provider = provider;

	dtrace_hash_add(dtrace_bymod, probe);
	dtrace_hash_add(dtrace_byfunc, probe);
	dtrace_hash_add(dtrace_byname, probe);

	if (provider != dtrace_provider)
		mutex_unlock(&dtrace_lock);

	return id;
}

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
	return idr_find(&dtrace_probe_idr, id);
}
