/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/bitmap.h>

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <alloca.h>
#include <unistd.h>
#include <errno.h>
#include <port.h>

#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_module.h>
#include <dt_string.h>
#include <dt_list.h>

/*
 * List of provider modules that register providers and probes.  A single
 * provider module may create multiple providers.
 */
const dt_provimpl_t *dt_providers[] = {
	&dt_dtrace,		/* list dt_dtrace first */
	&dt_cpc,
	&dt_fbt,
	&dt_io,
	&dt_ip,
	&dt_lockstat,
	&dt_proc,
	&dt_profile,
	&dt_rawtp,
	&dt_sched,
	&dt_sdt,
	&dt_syscall,
	&dt_uprobe,
	&dt_usdt,
	NULL
};

static uint32_t
dt_provider_hval(const dt_provider_t *pvp)
{
	return str2hval(pvp->desc.dtvd_name, 0);
}

static int
dt_provider_cmp(const dt_provider_t *p,
		const dt_provider_t *q)
{
	return strcmp(p->desc.dtvd_name, q->desc.dtvd_name);
}

DEFINE_HE_STD_LINK_FUNCS(dt_provider, dt_provider_t, he)

static void *
dt_provider_del_prov(dt_provider_t *head, dt_provider_t *pvp)
{
	head = dt_provider_del(head, pvp);

	if (pvp->pv_probes != NULL)
		dt_idhash_destroy(pvp->pv_probes);

	if (pvp->impl && pvp->impl->destroy)
		pvp->impl->destroy(pvp->pv_hdl, pvp->prv_data);

	dt_node_link_free(&pvp->pv_nodes);
	free(pvp->pv_xrefs);
	free(pvp);

	return head;
}

static dt_htab_ops_t dt_provider_htab_ops = {
	.hval = (htab_hval_fn)dt_provider_hval,
	.cmp = (htab_cmp_fn)dt_provider_cmp,
	.add = (htab_add_fn)dt_provider_add,
	.del = (htab_del_fn)dt_provider_del_prov,
	.next = (htab_next_fn)dt_provider_next
};

static dt_provider_t *
dt_provider_insert(dtrace_hdl_t *dtp, dt_provider_t *pvp)
{
	if (!dtp->dt_provs) {
		dtp->dt_provs = dt_htab_create(dtp, &dt_provider_htab_ops);
		if (dtp->dt_provs == NULL)
			return NULL;
	}

	if (dt_htab_insert(dtp->dt_provs, pvp) < 0) {
		free(pvp);
		return NULL;
	}

	return pvp;
}

dt_provider_t *
dt_provider_lookup(dtrace_hdl_t *dtp, const char *name)
{
	dt_provider_t tmpl;

	if ((strlen(name) + 1) > sizeof(tmpl.desc.dtvd_name))
		return NULL;

	strcpy(tmpl.desc.dtvd_name, name);
	return dt_htab_lookup(dtp->dt_provs, &tmpl);
}

dt_provider_t *
dt_provider_create(dtrace_hdl_t *dtp, const char *name,
		   const dt_provimpl_t *impl, const dtrace_pattr_t *pattr,
		   void *datap)
{
	dt_provider_t *pvp;

	if ((pvp = dt_zalloc(dtp, sizeof(dt_provider_t))) == NULL)
		goto nomem;

	strlcpy(pvp->desc.dtvd_name, name, DTRACE_PROVNAMELEN);
	pvp->impl = impl;
	pvp->pv_probes = dt_idhash_create(pvp->desc.dtvd_name, NULL, 0, 0);
	pvp->pv_gen = dtp->dt_gen;
	pvp->pv_hdl = dtp;
	pvp->prv_data = datap;
	dt_dprintf("creating provider %s\n", name);

	if (pvp->pv_probes == NULL)
		goto nomem;

	memcpy(&pvp->desc.dtvd_attr, pattr, sizeof(dtrace_pattr_t));

	return dt_provider_insert(dtp, pvp);

nomem:
	if (pvp)
		dt_free(dtp, pvp);

	dt_set_errno(dtp, EDT_NOMEM);
	return NULL;
}

int
dt_provider_xref(dtrace_hdl_t *dtp, dt_provider_t *pvp, id_t id)
{
	size_t oldsize = BT_SIZEOFMAP(pvp->pv_xrmax);
	size_t newsize = BT_SIZEOFMAP(dtp->dt_xlatorid);

	assert(id >= 0 && id < dtp->dt_xlatorid);

	if (newsize > oldsize) {
		ulong_t *xrefs = dt_zalloc(dtp, newsize);

		if (xrefs == NULL)
			return -1;

		memcpy(xrefs, pvp->pv_xrefs, oldsize);
		dt_free(dtp, pvp->pv_xrefs);

		pvp->pv_xrefs = xrefs;
		pvp->pv_xrmax = dtp->dt_xlatorid;
	}

	BT_SET(pvp->pv_xrefs, id);
	return 0;
}

int
dt_provider_discover(dtrace_hdl_t *dtp)
{
	int i, prid = dtp->dt_probe_id;

	/* Discover new probes. */
	for (i = 0; dt_providers[i]; i++) {
		if (dt_providers[i]->discover && dt_providers[i]->discover(dtp) < 0)
			return -1;        /* errno is already set */
	}

	/* Add them. */
	for ( ; prid < dtp->dt_probe_id; prid++) {
		dt_probe_t	*prp = dtp->dt_probes[prid];
		int		rc;

		dt_probe_enable(dtp, prp);

		if (prp->prov->impl->add_probe == NULL)
			continue;

		rc = prp->prov->impl->add_probe(dtp, prp);
		if (rc < 0)
			return rc;
	}

	return 0;
}
