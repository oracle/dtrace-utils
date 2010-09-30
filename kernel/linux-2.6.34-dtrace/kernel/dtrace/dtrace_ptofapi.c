/*
 * FILE:	dtrace_ptofapi.c
 * DESCRIPTION:	Dynamic Tracing: provider-to-framework API
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/idr.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "dtrace.h"

dtrace_provider_t	*dtrace_provider;

DEFINE_MUTEX(dtrace_lock);
DEFINE_MUTEX(dtrace_provider_lock);
DEFINE_MUTEX(dtrace_meta_lock);

/*
 * Register the calling provider with the DTrace core.  This should generally
 * be called by providers during module initialization.
 */
int dtrace_register(const char *name, const dtrace_pattr_t *pap, uint32_t priv,
		    cred_t *cr, const dtrace_pops_t *pops, void *arg,
		    dtrace_provider_id_t *idp)
{
	dtrace_provider_t	*provider;

	if (name == NULL || pap == NULL || pops == NULL || idp == NULL) {
		pr_warning("Failed to register provider %s: invalid args\n",
			   name ? name : "<NULL>");
		return -EINVAL;
	}

	if (name[0] == '\0' || dtrace_badname(name)) {
		pr_warning("Failed to register provider %s: invalid name\n",
			   name);
		return -EINVAL;
	}

	if ((pops->dtps_provide == NULL && pops->dtps_provide_module == NULL) ||
	    pops->dtps_enable == NULL || pops->dtps_disable == NULL ||
	    pops->dtps_destroy == NULL ||
	    ((pops->dtps_resume == NULL) != (pops->dtps_suspend == NULL))) {
		pr_warning("Failed to register provider %s: invalid ops\n",
			   name);
		return -EINVAL;
	}

	if (dtrace_badattr(&pap->dtpa_provider) ||
	    dtrace_badattr(&pap->dtpa_mod) ||
	    dtrace_badattr(&pap->dtpa_func) ||
	    dtrace_badattr(&pap->dtpa_name) ||
	    dtrace_badattr(&pap->dtpa_args)) {
		pr_warning("Failed to register provider %s: invalid "
			   "attributes\n", name);
		return -EINVAL;
	}

	if (priv & ~DTRACE_PRIV_ALL) {
		pr_warning("Failed to register provider %s: invalid privilege "
			   "attributes\n", name);
		return -EINVAL;
	}

	if ((priv & DTRACE_PRIV_KERNEL) &&
	    (priv & (DTRACE_PRIV_USER | DTRACE_PRIV_OWNER)) &&
	    pops->dtps_usermode == NULL) {
		pr_warning("Failed to register provider %s: need "
			   "dtps_usermode() op for given privilege "
			   "attributes\n", name);
		return -EINVAL;
	}

	provider = kmalloc(sizeof (dtrace_provider_t), GFP_KERNEL);
	provider->dtpv_name = kstrdup(name, GFP_KERNEL);
	provider->dtpv_attr = *pap;
	provider->dtpv_priv.dtpp_flags = priv;

	if (cr != NULL) {
		provider->dtpv_priv.dtpp_uid = get_cred(cr)->uid;
		put_cred(cr);
	}

	provider->dtpv_pops = *pops;

	if (pops->dtps_provide == NULL) {
		ASSERT(pops->dtps_provide_module != NULL);
		provider->dtpv_pops.dtps_provide =
		    (void (*)(void *, const dtrace_probedesc_t *))dtrace_nullop;
	}

	if (pops->dtps_provide_module == NULL) {
		ASSERT(pops->dtps_provide != NULL);
		provider->dtpv_pops.dtps_provide_module =
		    (void (*)(void *, struct module *))dtrace_nullop;
	}

	if (pops->dtps_suspend == NULL) {
		ASSERT(pops->dtps_resume != NULL);
		provider->dtpv_pops.dtps_suspend =
		    (void (*)(void *, dtrace_id_t, void *))dtrace_nullop;
		provider->dtpv_pops.dtps_resume =
		    (void (*)(void *, dtrace_id_t, void *))dtrace_nullop;
	}

	provider->dtpv_arg = arg;
	*idp = (dtrace_provider_id_t)provider;

	if (pops == &dtrace_provider_ops) {
		ASSERT(mutex_is_locked(&dtrace_provider_lock));
		ASSERT(mutex_is_locked(&dtrace_lock));
		ASSERT(dtrace_anon.dta_enabling == NULL);

		/*
		 * The DTrace provider must be at the head of the provider
		 * chain.
		 */
		provider->dtpv_next = dtrace_provider;
		dtrace_provider = provider;

		return 0;
	}

	mutex_lock(&dtrace_provider_lock);
	mutex_lock(&dtrace_lock);

	/*
	 * If there is at least one provider registered, we'll add this new one
	 * after the first provider.
	 */
	if (dtrace_provider != NULL) {
		provider->dtpv_next = dtrace_provider->dtpv_next;
		dtrace_provider->dtpv_next = provider;
	} else
		dtrace_provider = provider;

	if (dtrace_retained != NULL) {
		dtrace_enabling_provide(provider);

		/*
		 * We must now call dtrace_enabling_matchall() which needs to
		 * acquire FIXME(cpu_lock) and dtrace_lock.  We therefore need
		 * to drop our locks before calling it.
		 */
		mutex_unlock(&dtrace_lock);
		mutex_unlock(&dtrace_provider_lock);
		dtrace_enabling_matchall();

		return 0;
	}

	mutex_unlock(&dtrace_lock);
	mutex_unlock(&dtrace_provider_lock);

	return 0;
}
EXPORT_SYMBOL(dtrace_register);

/*
 * Check whether the given probe is still enabled for the given provider.
 */
static int dtrace_unregister_check(int id, void *p, void *data)
{
	dtrace_probe_t		*probe = (dtrace_probe_t *)p;
	dtrace_provider_t	*prov = (dtrace_provider_t *)data;

	if (probe->dtpr_provider != prov)
		return 0;

	if (probe->dtpr_ecb == NULL)
		return 0;

	return -EBUSY;
}

/*
 * Remove the given probe from the hash tables and the probe IDR.  The probes
 * are chained for further processing.
 */
static int dtrace_unregister_probe(int id, void *p, void *data)
{
	dtrace_probe_t	*probe = (dtrace_probe_t *)p;
	dtrace_probe_t	**first = (dtrace_probe_t **)data;

	dtrace_hash_remove(dtrace_bymod, probe);
	dtrace_hash_remove(dtrace_byfunc, probe);
	dtrace_hash_remove(dtrace_byname, probe);

	if (*first == NULL) {
		*first = probe;
		probe->dtpr_nextmod = NULL;
	} else {
		probe->dtpr_nextmod = *first;
		*first = probe;
	}

	return 0;
}

/*
 * Unregister the specified provider from the DTrace core.  This should be
 * called by provider during module cleanup.
 */
int dtrace_unregister(dtrace_provider_id_t id)
{
	dtrace_provider_t	*old = (dtrace_provider_t *)id;
	dtrace_provider_t	*prev = NULL;
	int			err, self = 0;
	dtrace_probe_t		*probe, *first = NULL;

	if (old->dtpv_pops.dtps_enable ==
	    (int (*)(void *, dtrace_id_t, void *))dtrace_enable_nullop) {
		/*
		 * When the provider is the DTrace core itself, we're called
		 * with locks already held.
		 */
		ASSERT(old == dtrace_provider);
		ASSERT(mutex_is_locked(&dtrace_provider_lock));
		ASSERT(mutex_is_locked(&dtrace_lock));

		self = 1;

		if (dtrace_provider->dtpv_next != NULL) {
			/*
			 * We cannot and should not remove the DTrace provider
			 * if there is any other provider left.
			 */
			return -EBUSY;
		}
	} else {
		mutex_lock(&dtrace_provider_lock);
		/* FIXME: mutex_lock(&mod_lock); */
		mutex_lock(&dtrace_lock);
	}

	/*
	 * If /dev/dtrace/dtrace is still held open by a process, or if there
	 * are anonymous probes that are still enabled, we refuse to deregister
	 * providers, unless the provider has been invalidated explicitly.
	 */
	if (!old->dtpv_defunct &&
	    (dtrace_opens || (dtrace_anon.dta_state != NULL &&
	     dtrace_anon.dta_state->dts_necbs > 0))) {
		if (!self) {
			mutex_unlock(&dtrace_lock);
			/* FIXME: mutex_unlock(&mod_lock); */
			mutex_unlock(&dtrace_provider_lock);
		}

		return -EBUSY;
	}

	/*
	 * Check whether any of the probes associated with this provider are
	 * still enabled (having at least one ECB).  If any are found, we
	 * cannot remove this provider.
	 */
	err = idr_for_each(&dtrace_probe_idr, dtrace_unregister_check, old);
	if (err < 0) {
		if (!self) {
			mutex_unlock(&dtrace_lock);
			/* FIXME: mutex_unlock(&mod_lock); */
			mutex_unlock(&dtrace_provider_lock);
		}

		return err;
	}

	/*
	 * All the probes associated with this provider are disabled.  We can
	 * safely remove these probes from the hashtables and the probe array.
	 * We chain all the probes together for further processing.
	 */
	idr_for_each(&dtrace_probe_idr, dtrace_unregister_probe, &first);

	/*
	 * The probes associated with the provider have been removed.  Ensure
	 * synchronization on probe IDR processing.
	 */
	dtrace_sync();

	/*
	 * Now get rid of the actual probes.
	 */
	for (probe = first; probe != NULL; probe = first) {
		int	probe_id = probe->dtpr_id;

		first = probe->dtpr_nextmod;

		old->dtpv_pops.dtps_destroy(old->dtpv_arg, probe_id,
					    probe->dtpr_arg);

		kfree(probe->dtpr_mod);
		kfree(probe->dtpr_func);
		kfree(probe->dtpr_name);
		kfree(probe);

		idr_remove(&dtrace_probe_idr, probe_id);
	}

	if ((prev = dtrace_provider) == old) {
		/*
		 * We are removing the provider at the head of the chain.
		 */
		ASSERT(self);
		ASSERT(old->dtpv_next == NULL);

		dtrace_provider = old->dtpv_next;
	} else {
		while (prev != NULL && prev->dtpv_next != old)
			prev = prev->dtpv_next;

		if (prev == NULL) {
			pr_err("Attempt to unregister non-existent DTrace "
			       "provider %p\n", (void *)id);
			BUG();
		}

		prev->dtpv_next = old->dtpv_next;
	}

	if (!self) {
		mutex_unlock(&dtrace_lock);
		/* FIXME: mutex_unlock(&mod_lock); */
		mutex_unlock(&dtrace_provider_lock);
	}

	kfree(old->dtpv_name);
	kfree(old);

	return 0;
}
EXPORT_SYMBOL(dtrace_unregister);
