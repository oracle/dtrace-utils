/*
 * FILE:	dtrace_probe.c
 * DESCRIPTION:	Dynamic Tracing: probe management functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>
#include <linux/slab.h>

#include "dtrace.h"

int		dtrace_nprobes;
dtrace_probe_t	**dtrace_probes;

/*
 * Create a new probe.
 */
dtrace_id_t dtrace_probe_create(dtrace_provider_id_t prov, const char *mod,
				const char *func, const char *name,
				int aframes, void *arg)
{
	dtrace_probe_t		*probe, **probes;
	dtrace_provider_t	*provider = (dtrace_provider_t *)prov;
	dtrace_id_t		id;

	if (provider == dtrace_provider)
		BUG_ON(!mutex_is_locked(&dtrace_lock));
	else
		mutex_lock(&dtrace_lock);

        /*
         * KVH FIXME: I am not too sure this is the best way to handle
         * aggregate ids.  Essentially, a 1 byte allocation is performed,
         * resulting in a unique virtual address that is converted into an
         * integer value and used as id.  On Linux, I believe that this
         * technique results in overhead due to the allocation.  I changed this
         * to use kmalloc to aovid the vmalloc overhead (since vmalloc aligns
         * all allocations on a page boundary).
         */
	id = (dtrace_id_t)(uintptr_t)kmalloc(1, GFP_KERNEL);

	probe = kzalloc(sizeof (dtrace_probe_t), GFP_KERNEL);
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

	if (id - 1 >= dtrace_nprobes) {
		size_t	osize = dtrace_nprobes * sizeof (dtrace_probe_t *);
		size_t	nsize = osize << 1;

		if (nsize == 0) {
			BUG_ON(osize != 0);
			BUG_ON(dtrace_probes != NULL);

			nsize = sizeof (dtrace_probe_t *);
		}

		probes = kzalloc(nsize, GFP_KERNEL);

		if (dtrace_probes == NULL) {
			BUG_ON(osize != 0);

			dtrace_probes = probes;
			dtrace_nprobes = 1;
		} else {
			dtrace_probe_t	**oprobes = dtrace_probes;

			memcpy(probes, oprobes, osize);
			dtrace_membar_producer();
			dtrace_probes = probes;
			dtrace_sync();

			/*
			 * All CPUs can now see the new probes array; time to
			 * get rid of the old one.
			 */
			kfree(oprobes);

			dtrace_nprobes <<= 1;
		}

		BUG_ON(id - 1 < dtrace_nprobes);
	}

	BUG_ON(dtrace_probes[id - 1] != NULL);

	dtrace_probes[id - 1] = probe;

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
	if (id == 0 || id > dtrace_nprobes)
		return NULL;

	return dtrace_probes[id - 1];
}
