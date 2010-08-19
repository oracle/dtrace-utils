/*
 * FILE:	dtrace_ptofapi.c
 * DESCRIPTION:	Dynamic Tracing: provider-to-framework API
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/module.h>
#include <linux/slab.h>

#include "dtrace.h"

dtrace_provider_t	*dtrace_provider;

DEFINE_MUTEX(dtrace_lock);
DEFINE_MUTEX(dtrace_provider_lock);
DEFINE_MUTEX(dtrace_meta_lock);

static void dtrace_nullop(void)
{
}

static int dtrace_enable_nullop(void)
{
	return 0;
}

static dtrace_pops_t		dtrace_provider_ops = {
	(void (*)(void *, const dtrace_probedesc_t *))dtrace_nullop,
	(void (*)(void *, struct module *))dtrace_nullop,
	(int (*)(void *, dtrace_id_t, void *))dtrace_enable_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	NULL,
	NULL,
	NULL,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop
};

int dtrace_register(const char *name, const dtrace_pattr_t *pap, uint32_t priv,
		    cred_t *cr, const dtrace_pops_t *pops, void *arg,
		    dtrace_provider_id_t *idp)
{
	dtrace_provider_t	*provider;

	if (name == NULL || pap == NULL || pops == NULL || idp == NULL)
		return -EINVAL;

	if (name[0] == '\0' || dtrace_badname(name))
		return -EINVAL;

	if ((pops->dtps_provide == NULL && pops->dtps_provide_module == NULL) ||
	    pops->dtps_enable == NULL || pops->dtps_disable == NULL ||
	    pops->dtps_destroy == NULL ||
	    ((pops->dtps_resume == NULL) != (pops->dtps_suspend == NULL)))
		return -EINVAL;

	if (dtrace_badattr(&pap->dtpa_provider) ||
	    dtrace_badattr(&pap->dtpa_mod) ||
	    dtrace_badattr(&pap->dtpa_func) ||
	    dtrace_badattr(&pap->dtpa_name) ||
	    dtrace_badattr(&pap->dtpa_args))
		return -EINVAL;

	if (priv & ~DTRACE_PRIV_ALL)
		return -EINVAL;

	if ((priv & DTRACE_PRIV_KERNEL) &&
	    (priv & (DTRACE_PRIV_USER | DTRACE_PRIV_OWNER)) &&
	    pops->dtps_usermode == NULL)
		return -EINVAL;

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
		WARN_ON(pops->dtps_provide_module == NULL);
		provider->dtpv_pops.dtps_provide =
		    (void (*)(void *, const dtrace_probedesc_t *))dtrace_nullop;
	}

	if (pops->dtps_provide_module == NULL) {
		WARN_ON(pops->dtps_provide == NULL);
		provider->dtpv_pops.dtps_provide_module =
		    (void (*)(void *, struct module *))dtrace_nullop;
	}

	if (pops->dtps_suspend == NULL) {
		WARN_ON(pops->dtps_resume == NULL);
		provider->dtpv_pops.dtps_suspend =
		    (void (*)(void *, dtrace_id_t, void *))dtrace_nullop;
		provider->dtpv_pops.dtps_resume =
		    (void (*)(void *, dtrace_id_t, void *))dtrace_nullop;
	}

	provider->dtpv_arg = arg;
	*idp = (dtrace_provider_id_t)provider;

	if (pops == &dtrace_provider_ops) {
		provider->dtpv_next = dtrace_provider;
		dtrace_provider = provider;

		return 0;
	}

	mutex_lock(&dtrace_provider_lock);
	mutex_lock(&dtrace_lock);

	if (dtrace_provider != NULL) {
		provider->dtpv_next = dtrace_provider->dtpv_next;
		dtrace_provider->dtpv_next = provider;
	} else
		dtrace_provider = provider;

	if (dtrace_retained != NULL) {
		dtrace_enabling_provide(provider);

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

int dtrace_unregister(dtrace_provider_id_t id)
{
	return 0;
}
EXPORT_SYMBOL(dtrace_unregister);
