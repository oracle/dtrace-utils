/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
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

#include <dt_probe.h>
#include <dt_module.h>
#include <dt_string.h>
#include <dt_htab.h>
#include <dt_list.h>

typedef struct dt_probeclause {
	dt_list_t	list;
	dt_ident_t	*clause;
} dt_probeclause_t;

#define DEFINE_HE_FUNCS(id) \
	static uint32_t id##_hval(const dt_probe_t *probe) \
	{ \
		return str2hval(probe->desc->id, 0); \
	} \
	\
	static int id##_cmp(const dt_probe_t *p, \
			    const dt_probe_t *q) \
	{ \
		return strcmp(p->desc->id, q->desc->id); \
	} \
	\
	DEFINE_HE_LINK_FUNCS(id)

#define DEFINE_HE_LINK_FUNCS(id) \
	static dt_probe_t *id##_add(dt_probe_t *head, dt_probe_t *new) \
	{ \
		if (!head) \
			return new; \
	\
		new->he_##id.next = head; \
		head->he_##id.prev = new; \
	\
		return new; \
	} \
	\
	static dt_probe_t *id##_del(dt_probe_t *head, dt_probe_t *probe) \
	{ \
		dt_probe_t *prev = probe->he_##id.prev; \
		dt_probe_t *next = probe->he_##id.next; \
	\
		if (head == probe) { \
			if (!next) \
				return NULL; \
	\
			head = next; \
			head->he_##id.prev = NULL; \
			probe->he_##id.next = NULL; \
	\
			return head; \
		} \
	\
		if (!next) { \
			prev->he_##id.next = NULL; \
			probe->he_##id.prev = NULL; \
	\
			return head; \
	} \
	\
		prev->he_##id.next = next; \
		next->he_##id.prev = prev; \
		probe->he_##id.prev = probe->he_##id.next = NULL; \
	\
		return head; \
	}

#define DEFINE_HTAB_OPS(id) \
	static dt_htab_ops_t	id##_htab_ops = { \
		.hval = (htab_hval_fn)id##_hval, \
		.cmp = (htab_cmp_fn)id##_cmp, \
		.add = (htab_add_fn)id##_add, \
		.del = (htab_del_fn)id##_del, \
	};

DEFINE_HE_FUNCS(prv)
DEFINE_HE_FUNCS(mod)
DEFINE_HE_FUNCS(fun)
DEFINE_HE_FUNCS(prb)

/*
 * Calculate the hash value of a probe as the cummulative hash value of the
 * FQN.
 */
static uint32_t fqn_hval(const dt_probe_t *probe)
{
	uint32_t	hval = 0;

	hval = str2hval(probe->desc->prv, hval);
	hval = str2hval(":", hval);
	hval = str2hval(probe->desc->mod, hval);
	hval = str2hval(":", hval);
	hval = str2hval(probe->desc->fun, hval);
	hval = str2hval(":", hval);
	hval = str2hval(probe->desc->prb, hval);

	return hval;
}

/* Compare two probes based on the FQN. */
static int fqn_cmp(const dt_probe_t *p, const dt_probe_t *q)
{
	int	rc;

	rc = strcmp(p->desc->prv, q->desc->prv);
	if (rc)
		return rc;
	rc = strcmp(p->desc->mod, q->desc->mod);
	if (rc)
		return rc;
	rc = strcmp(p->desc->fun, q->desc->fun);
	if (rc)
		return rc;
	rc = strcmp(p->desc->prb, q->desc->prb);
	if (rc)
		return rc;

	return 0;
}

DEFINE_HE_LINK_FUNCS(fqn)

DEFINE_HTAB_OPS(prv)
DEFINE_HTAB_OPS(mod)
DEFINE_HTAB_OPS(fun)
DEFINE_HTAB_OPS(prb)
DEFINE_HTAB_OPS(fqn)

static uint8_t
dt_probe_argmap(dt_node_t *xnp, dt_node_t *nnp)
{
	uint8_t i;

	for (i = 0; nnp != NULL; i++) {
		if (nnp->dn_string != NULL &&
		    strcmp(nnp->dn_string, xnp->dn_string) == 0)
			break;
		else
			nnp = nnp->dn_list;
	}

	return i;
}

static dt_node_t *
alloc_arg_nodes(dtrace_hdl_t *dtp, dt_provider_t *pvp, int argc)
{
	dt_node_t	*dnp = NULL;
	int		i;

	for (i = 0; i < argc; i++) {
		if ((dnp = dt_node_xalloc(dtp, DT_NODE_TYPE)) == NULL)
			return NULL;

		dnp->dn_link = pvp->pv_nodes;
		pvp->pv_nodes = dnp;
	}

	return dnp;
}

static void
dt_probe_alloc_args(dt_probe_t *prp, int nargc, int xargc)
{
	dt_provider_t	*pvp = prp->prov;
	dtrace_hdl_t	*dtp = pvp->pv_hdl;
	dt_node_t	*nargs = NULL, *xargs = NULL;
	int		i;

	prp->nargs = alloc_arg_nodes(dtp, prp->prov, nargc);
	prp->nargv = dt_calloc(dtp, nargc, sizeof(dt_node_t *));
	prp->nargc = nargc;
	prp->xargs = alloc_arg_nodes(dtp, prp->prov, xargc);
	prp->xargv = dt_calloc(dtp, xargc, sizeof(dt_node_t *));
	prp->xargc = xargc;
	prp->mapping = dt_calloc(dtp, xargc, sizeof(uint8_t));
	prp->argv = dt_calloc(dtp, xargc, sizeof(dtrace_typeinfo_t));
	prp->argc = xargc;

	for (i = 0, xargs = prp->xargs;
	     i < xargc;
	     i++, xargs = xargs->dn_link) {
		prp->mapping[i] = i;
		prp->xargv[i] = xargs;
		prp->argv[i].dtt_object = NULL;
		prp->argv[i].dtt_ctfp = NULL;
		prp->argv[i].dtt_type = CTF_ERR;
	}

	for (i = 0, nargs = prp->nargs;
	     i < nargc;
	     i++, nargs = nargs->dn_link) {
		prp->nargv[i] = nargs;
	}
}

static size_t
dt_probe_keylen(const dtrace_probedesc_t *pdp)
{
	return strlen(pdp->mod) + 1 + strlen(pdp->fun) + 1 +
		strlen(pdp->prb) + 1;
}

static char *
dt_probe_key(const dtrace_probedesc_t *pdp, char *s)
{
	snprintf(s, INT_MAX, "%s:%s:%s", pdp->mod, pdp->fun, pdp->prb);
	return s;
}

/*
 * If a probe was discovered from the kernel, ask dtrace(7D) for a description
 * of each of its arguments, including native and translated types.
 */
static dt_probe_t *
dt_probe_discover(dt_provider_t *pvp, const dtrace_probedesc_t *pdp)
{
#ifdef FIXME
	dtrace_hdl_t *dtp = pvp->pv_hdl;
	char *name = dt_probe_key(pdp, alloca(dt_probe_keylen(pdp)));

	dt_node_t *xargs, *nargs;
	dt_ident_t *idp;
	dt_probe_t *prp;

	dtrace_typeinfo_t dtt;
	int i, nc, xc;

	int adc = _dtrace_argmax;
	dt_argdesc_t *adv = alloca(sizeof(dt_argdesc_t) * adc);
	dt_argdesc_t *adp = adv;

	assert(strcmp(pvp->desc.dtvd_name, pdp->prv) == 0);
	assert(pdp->id != DTRACE_IDNONE);

	dt_dprintf("discovering probe %s:%s id=%d\n",
		   pvp->desc.dtvd_name, name, pdp->id);

	for (nc = -1, i = 0; i < adc; i++, adp++) {
		memset(adp, 0, sizeof(dt_argdesc_t));
		adp->ndx = i;
		adp->id = pdp->id;

		if (dt_ioctl(dtp, DTRACEIOC_PROBEARG, adp) != 0) {
			dt_set_errno(dtp, errno);
			return NULL;
		}

		if (adp->ndx == DTRACE_ARGNONE)
			break; /* all argument descs have been retrieved */

		nc = MAX(nc, adp->mapping);
	}

	xc = i;
	nc++;

	/*
	 * Now that we have discovered the number of native and translated
	 * arguments from the argument descriptions, allocate a new probe ident
	 * and corresponding dt_probe_t and hash it into the provider.
	 */
	xargs = dt_probe_alloc_args(pvp, xc);
	nargs = dt_probe_alloc_args(pvp, nc);

	if ((xc != 0 && xargs == NULL) || (nc != 0 && nargs == NULL))
		return NULL; /* dt_errno is set for us */

	idp = dt_ident_create(name, DT_IDENT_PROBE, DT_IDFLG_ORPHAN, pdp->id,
			      _dtrace_defattr, 0, &dt_idops_probe, NULL,
			      dtp->dt_gen);

	if (idp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	prp = dt_probe_create(dtp, idp, 2, nargs, nc, xargs, xc);
	if (prp == NULL) {
		dt_ident_destroy(idp);
		return NULL;
	}

	dt_probe_declare(pvp, prp);

	/*
	 * Once our new dt_probe_t is fully constructed, iterate over the
	 * cached argument descriptions and assign types to prp->nargv[]
	 * and prp->xargv[] and assign mappings to prp->mapping[].
	 */
	for (adp = adv, i = 0; i < xc; i++, adp++) {
		if (dtrace_type_strcompile(dtp,
		    adp->native, &dtt) != 0) {
			dt_dprintf("failed to resolve input type %s "
			    "for %s:%s arg #%d: %s\n", adp->native,
			    pvp->desc.dtvd_name, name, i + 1,
			    dtrace_errmsg(dtp, dtrace_errno(dtp)));

			dtt.dtt_object = NULL;
			dtt.dtt_ctfp = NULL;
			dtt.dtt_type = CTF_ERR;
		} else {
			dt_node_type_assign(prp->nargv[adp->mapping],
			    dtt.dtt_ctfp, dtt.dtt_type);
		}

		if (dtt.dtt_type != CTF_ERR && (adp->xlate[0] == '\0' ||
		    strcmp(adp->native, adp->xlate) == 0)) {
			dt_node_type_propagate(prp->nargv[
			    adp->mapping], prp->xargv[i]);
		} else if (dtrace_type_strcompile(dtp,
		    adp->xlate, &dtt) != 0) {
			dt_dprintf("failed to resolve output type %s "
			    "for %s:%s arg #%d: %s\n", adp->xlate,
			    pvp->desc.dtvd_name, name, i + 1,
			    dtrace_errmsg(dtp, dtrace_errno(dtp)));

			dtt.dtt_object = NULL;
			dtt.dtt_ctfp = NULL;
			dtt.dtt_type = CTF_ERR;
		} else {
			dt_node_type_assign(prp->xargv[i],
			    dtt.dtt_ctfp, dtt.dtt_type);
		}

		prp->mapping[i] = adp->mapping;
		prp->argv[i] = dtt;
	}

	return prp;
#else
	return NULL;
#endif
}

/*
 * Lookup a probe declaration based on a known provider and full or partially
 * specified module, function, and name.  If the probe is not known to us yet,
 * ask dtrace(7D) to match the description and then cache any useful results.
 */
dt_probe_t *
dt_probe_lookup2(dt_provider_t *pvp, const char *s)
{
	dtrace_hdl_t *dtp = pvp->pv_hdl;
	dtrace_probedesc_t pd;
	dt_ident_t *idp;
	size_t keylen;
	char *key;

	if (dtrace_str2desc(dtp, DTRACE_PROBESPEC_NAME, s, &pd) != 0)
		return NULL; /* dt_errno is set for us */

	keylen = dt_probe_keylen(&pd);
	key = dt_probe_key(&pd, alloca(keylen));

	/*
	 * If the probe is already declared, then return the dt_probe_t from
	 * the existing identifier.  This could come from a static declaration
	 * or it could have been cached from an earlier call to this function.
	 */
	if ((idp = dt_idhash_lookup(pvp->pv_probes, key)) != NULL)
		return idp->di_data;

	/*
	 * If the probe isn't known, use the probe description computed above
	 * to ask dtrace(7D) to find the first matching probe.
	 */
	if (dt_ioctl(dtp, DTRACEIOC_PROBEMATCH, &pd) == 0)
		return dt_probe_discover(pvp, &pd);

	if (errno == ESRCH || errno == EBADF)
		dt_set_errno(dtp, EDT_NOPROBE);
	else
		dt_set_errno(dtp, errno);

	return NULL;
}

dt_probe_t *
dt_probe_create(dtrace_hdl_t *dtp, dt_ident_t *idp, int protoc,
		dt_node_t *nargs, uint_t nargc, dt_node_t *xargs, uint_t xargc)
{
	dt_module_t *dmp;
	dt_probe_t *prp;
	const char *p;
	uint_t i;

	assert(idp->di_kind == DT_IDENT_PROBE);
	assert(idp->di_data == NULL);

	/*
	 * If only a single prototype is given, set xargc/s to nargc/s to
	 * simplify subsequent use.  Note that we can have one or both of nargs
	 * and xargs be specified but set to NULL, indicating a void prototype.
	 */
	if (protoc < 2) {
		assert(xargs == NULL);
		assert(xargc == 0);
		xargs = nargs;
		xargc = nargc;
	}

	prp = dt_zalloc(dtp, sizeof(dt_probe_t));
	if (prp == NULL)
		return NULL;

	prp->prov = NULL;
	prp->pr_ident = idp;

	p = strrchr(idp->di_name, ':');
	assert(p != NULL);
	prp->pr_name = p + 1;

	prp->nargs = nargs;
	prp->nargv = dt_calloc(dtp, nargc, sizeof(dt_node_t *));
	prp->nargc = nargc;
	prp->xargs = xargs;
	prp->xargv = dt_calloc(dtp, xargc, sizeof(dt_node_t *));
	prp->xargc = xargc;
	prp->mapping = dt_calloc(dtp, xargc, sizeof(uint8_t));
	prp->pr_inst = NULL;
	prp->argv = dt_calloc(dtp, xargc, sizeof(dtrace_typeinfo_t));
	prp->argc = xargc;

	if ((prp->nargc != 0 && prp->nargv == NULL) ||
	    (prp->xargc != 0 && prp->xargv == NULL) ||
	    (prp->xargc != 0 && prp->mapping == NULL) ||
	    (prp->argc != 0 && prp->argv == NULL)) {
		dt_probe_destroy(prp);
		return NULL;
	}

	for (i = 0; i < xargc; i++, xargs = xargs->dn_list) {
		if (xargs->dn_string != NULL)
			prp->mapping[i] = dt_probe_argmap(xargs, nargs);
		else
			prp->mapping[i] = i;

		prp->xargv[i] = xargs;

		if ((dmp = dt_module_lookup_by_ctf(dtp,
		    xargs->dn_ctfp)) != NULL)
			prp->argv[i].dtt_object = dmp->dm_name;
		else
			prp->argv[i].dtt_object = NULL;

		prp->argv[i].dtt_ctfp = xargs->dn_ctfp;
		prp->argv[i].dtt_type = xargs->dn_type;
	}

	for (i = 0; i < nargc; i++, nargs = nargs->dn_list)
		prp->nargv[i] = nargs;

	idp->di_data = prp;

	return prp;
}

void
dt_probe_declare(dt_provider_t *pvp, dt_probe_t *prp)
{
	assert(prp->pr_ident->di_kind == DT_IDENT_PROBE);
	assert(prp->pr_ident->di_data == prp);
	assert(prp->prov == NULL);

	if (prp->xargs != prp->nargs)
		pvp->pv_flags &= ~DT_PROVIDER_INTF;

	prp->prov = pvp;
	dt_idhash_xinsert(pvp->pv_probes, prp->pr_ident);
}

void
dt_probe_destroy(dt_probe_t *prp)
{
	dt_probeclause_t	*pcp, *pcp_next;
	dt_probe_instance_t	*pip, *pip_next;
	dtrace_hdl_t		*dtp;

	if (prp->prov != NULL)
		dtp = prp->prov->pv_hdl;
	else
		dtp = yypcb->pcb_hdl;

	if (prp->desc) {
		dt_htab_delete(dtp->dt_byprv, prp);
		dt_htab_delete(dtp->dt_bymod, prp);
		dt_htab_delete(dtp->dt_byfun, prp);
		dt_htab_delete(dtp->dt_byprb, prp);
		dt_htab_delete(dtp->dt_byfqn, prp);
	}

	if (prp->prov && prp->prov->impl && prp->prov->impl->probe_fini)
		prp->prov->impl->probe_fini(dtp, prp);

	dt_node_list_free(&prp->nargs);
	dt_node_list_free(&prp->xargs);

	dt_free(dtp, prp->nargv);
	dt_free(dtp, prp->xargv);

	for (pcp = dt_list_next(&prp->clauses); pcp != NULL; pcp = pcp_next) {
		pcp_next = dt_list_next(pcp);
		dt_free(dtp, pcp);
	}

	for (pip = prp->pr_inst; pip != NULL; pip = pip_next) {
		pip_next = pip->pi_next;
		dt_free(dtp, pip->pi_offs);
		dt_free(dtp, pip->pi_enoffs);
		dt_free(dtp, pip);
	}

	dt_free(dtp, prp->mapping);
	dt_free(dtp, prp->argv);
	if (prp->desc) {
		dt_free(dtp, (void *)prp->desc->prv);
		dt_free(dtp, (void *)prp->desc->mod);
		dt_free(dtp, (void *)prp->desc->fun);
		dt_free(dtp, (void *)prp->desc->prb);
		dt_free(dtp, (void *)prp->desc);
	}
	dt_free(dtp, prp);
}

int
dt_probe_define(dt_provider_t *pvp, dt_probe_t *prp, const char *fname,
		const char *rname, uint32_t offset, int isenabled)
{
	dtrace_hdl_t *dtp = pvp->pv_hdl;
	dt_probe_instance_t *pip;
	uint32_t **offs;
	uint_t *noffs, *maxoffs;

	assert(fname != NULL);

	for (pip = prp->pr_inst; pip != NULL; pip = pip->pi_next) {
		if (strcmp(pip->pi_fname, fname) == 0 &&
		    ((rname == NULL && pip->pi_rname[0] == '\0') ||
		    (rname != NULL && strcmp(pip->pi_rname, rname)) == 0))
			break;
	}

	if (pip == NULL) {
		if ((pip = dt_zalloc(dtp, sizeof(*pip))) == NULL)
			return -1;

		if ((pip->pi_offs = dt_zalloc(dtp,
		    sizeof(uint32_t))) == NULL) {
			dt_free(dtp, pip);
			return -1;
		}

		if ((pip->pi_enoffs = dt_zalloc(dtp,
		    sizeof(uint32_t))) == NULL) {
			dt_free(dtp, pip->pi_offs);
			dt_free(dtp, pip);
			return -1;
		}

		strlcpy(pip->pi_fname, fname, sizeof(pip->pi_fname));
		if (rname != NULL) {
			if (strlen(rname) + 1 > sizeof(pip->pi_rname)) {
				dt_free(dtp, pip->pi_offs);
				dt_free(dtp, pip);
				return dt_set_errno(dtp, EDT_COMPILER);
			}
			strcpy(pip->pi_rname, rname);
		}

		pip->pi_noffs = 0;
		pip->pi_maxoffs = 1;
		pip->pi_nenoffs = 0;
		pip->pi_maxenoffs = 1;

		pip->pi_next = prp->pr_inst;

		prp->pr_inst = pip;
	}

	if (isenabled) {
		offs = &pip->pi_enoffs;
		noffs = &pip->pi_nenoffs;
		maxoffs = &pip->pi_maxenoffs;
	} else {
		offs = &pip->pi_offs;
		noffs = &pip->pi_noffs;
		maxoffs = &pip->pi_maxoffs;
	}

	if (*noffs == *maxoffs) {
		uint_t new_max = *maxoffs * 2;
		uint32_t *new_offs = dt_calloc(dtp, new_max, sizeof(uint32_t));

		if (new_offs == NULL)
			return -1;

		memcpy(new_offs, *offs, sizeof(uint32_t) * *maxoffs);

		dt_free(dtp, *offs);
		*maxoffs = new_max;
		*offs = new_offs;
	}

	dt_dprintf("defined probe %s %s:%s %s() +0x%x (%s)\n",
	    isenabled ? "(is-enabled)" : "",
	    pvp->desc.dtvd_name, prp->pr_ident->di_name, fname, offset,
	    rname != NULL ? rname : fname);

	assert(*noffs < *maxoffs);
	(*offs)[(*noffs)++] = offset;

	return 0;
}

/*
 * Lookup the dynamic translator type tag for the specified probe argument and
 * assign the type to the specified node.  If the type is not yet defined, add
 * it to the "D" module's type container as a typedef for an unknown type.
 */
dt_node_t *
dt_probe_tag(dt_probe_t *prp, uint_t argn, dt_node_t *dnp)
{
	dtrace_hdl_t *dtp = prp->prov->pv_hdl;
	dtrace_typeinfo_t dtt;
	size_t len;
	char *tag;

	len = snprintf(NULL, 0, "__dtrace_%s___%s_arg%u",
	    prp->prov->desc.dtvd_name, prp->pr_name, argn);

	tag = alloca(len + 1);

	snprintf(tag, len + 1, "__dtrace_%s___%s_arg%u",
	    prp->prov->desc.dtvd_name, prp->pr_name, argn);

	if (dtrace_lookup_by_type(dtp, DTRACE_OBJ_DDEFS, tag, &dtt) != 0) {
		dtt.dtt_object = DTRACE_OBJ_DDEFS;
		dtt.dtt_ctfp = DT_DYN_CTFP(dtp);
		dtt.dtt_type = ctf_add_typedef(DT_DYN_CTFP(dtp),
		    CTF_ADD_ROOT, tag, DT_DYN_TYPE(dtp));

		if (dtt.dtt_type == CTF_ERR ||
		    ctf_update(dtt.dtt_ctfp) == CTF_ERR) {
			xyerror(D_UNKNOWN, "cannot define type %s: %s\n",
			    tag, ctf_errmsg(ctf_errno(dtt.dtt_ctfp)));
		}
	}

	memset(dnp, 0, sizeof(dt_node_t));
	dnp->dn_kind = DT_NODE_TYPE;

	dt_node_type_assign(dnp, dtt.dtt_ctfp, dtt.dtt_type);
	dt_node_attr_assign(dnp, _dtrace_defattr);

	return dnp;
}

dt_probe_t *
dt_probe_insert(dtrace_hdl_t *dtp, dt_provider_t *prov, const char *prv,
		const char *mod, const char *fun, const char *prb, void *datap)
{
	dt_probe_t		*prp;
	dtrace_probedesc_t	*desc;

	/* If necessary, grow the probes array. */
	if (dtp->dt_probe_id + 1 > dtp->dt_probes_sz) {
		dt_probe_t **nprobes;
		uint32_t nprobes_sz = dtp->dt_probes_sz + 1024;

		if (nprobes_sz < dtp->dt_probes_sz) {	/* overflow */
			dt_set_errno(dtp, EDT_NOMEM);
			goto err;
		}

		nprobes = dt_calloc(dtp, nprobes_sz, sizeof(dt_probe_t *));
		if (nprobes == NULL)
			goto err;

		if (dtp->dt_probes)
			memcpy(nprobes, dtp->dt_probes,
			       dtp->dt_probes_sz * sizeof(dt_probe_t *));

		free(dtp->dt_probes);
		dtp->dt_probes = nprobes;
		dtp->dt_probes_sz = nprobes_sz;
	}

	/* Allocate the new probe and fill in its basic info. */
	if ((prp = dt_zalloc(dtp, sizeof(dt_probe_t))) == NULL)
		goto err;

	if ((desc = dt_alloc(dtp, sizeof(dtrace_probedesc_t))) == NULL) {
		dt_free(dtp, prp);
		goto err;
	}

	desc->id = dtp->dt_probe_id++;
	desc->prv = strdup(prv);
	desc->mod = strdup(mod);
	desc->fun = strdup(fun);
	desc->prb = strdup(prb);

	prp->desc = desc;
	prp->prov = prov;
	prp->prv_data = datap;

	dt_htab_insert(dtp->dt_byprv, prp);
	dt_htab_insert(dtp->dt_bymod, prp);
	dt_htab_insert(dtp->dt_byfun, prp);
	dt_htab_insert(dtp->dt_byprb, prp);
	dt_htab_insert(dtp->dt_byfqn, prp);

	dtp->dt_probes[dtp->dt_probe_id - 1] = prp;

	return prp;

err:
	if (datap && prov->impl->probe_destroy)
		prov->impl->probe_destroy(dtp, datap);

	return NULL;
}

static int
dt_probe_gmatch(const dt_probe_t *prp, dtrace_probedesc_t *pdp)
{
#define MATCH_ONE(nam, n)						\
	if (pdp->nam) {							\
		if (pdp->id & (1 << n)) {				\
			if (!dt_gmatch(prp->desc->nam, pdp->nam))	\
				return 0;				\
		} else if (strcmp(prp->desc->nam, pdp->nam) != 0)	\
				return 0;				\
	}

	MATCH_ONE(prv, 3)
	MATCH_ONE(mod, 2)
	MATCH_ONE(fun, 1)
	MATCH_ONE(prb, 0)

	return 1;
}

/*
 * Look for a probe that matches the probe description in 'pdp'.
 *
 * If no probe is found, this function will return NULL (dt_errno will be set).
 *
 * If a probe id is provided in the probe description, a direct lookup can be
 * performed in the probe array.
 *
 * If a probe description is provided without any glob elements, a lookup can
 * be performed on the fully qualified probe name (htab lookup in dt_byfqn).
 *
 * If a probe description is provided with a mix of exact and glob elements,
 * a htab lookup is performed on the element that is commonly known to be the
 * most restrictive (dt_byfun is usually best, then dt_byprb, then dt_bymod,
 * and finally dt_byprv).  Further matching is then done on just the probes in
 * that htab bucket.  Elements that must match exactly are compared first.
 *
 * More than one probe might match, but only one probe will be returned.
 */
dt_probe_t *
dt_probe_lookup(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	dt_probe_t	*prp;
	dt_probe_t	tmpl;
	int		p_is_glob, m_is_glob, f_is_glob, n_is_glob;

	/*
	 * If a probe id is provided, we can do a direct lookup.
	 */
	if (pdp->id != DTRACE_IDNONE) {
		if (pdp->id >= dtp->dt_probe_id)
			goto no_probe;

		prp = dtp->dt_probes[pdp->id];
		if (!prp)
			goto no_probe;

		return prp;
	}

	tmpl.desc = pdp;

	p_is_glob = pdp->prv[0] == '\0' || strisglob(pdp->prv);
	m_is_glob = pdp->mod[0] == '\0' || strisglob(pdp->mod);
	f_is_glob = pdp->fun[0] == '\0' || strisglob(pdp->fun);
	n_is_glob = pdp->prb[0] == '\0' || strisglob(pdp->prb);

	/*
	 * If an exact (fully qualified) probe description is provided, a
	 * simple htab lookup in dtp->dt_byfqn will suffice.
	 */
	if (p_is_glob + m_is_glob + f_is_glob + n_is_glob == 0) {
		prp = dt_htab_lookup(dtp->dt_byfqn, &tmpl);
		if (!prp)
			goto no_probe;

		return prp;
	}

	/*
	 * If at least one element is specified as a string to match exactly,
	 * use that to consult its respective htab.  If all elements are
	 * specified as glob patterns (or the empty string), we need to loop
	 * through all probes and look for a match.
	 */
	if (!f_is_glob)
		prp = dt_htab_lookup(dtp->dt_byfun, &tmpl);
	else if (!n_is_glob)
		prp = dt_htab_lookup(dtp->dt_byprb, &tmpl);
	else if (!m_is_glob)
		prp = dt_htab_lookup(dtp->dt_bymod, &tmpl);
	else if (!p_is_glob)
		prp = dt_htab_lookup(dtp->dt_byprv, &tmpl);
	else {
		int			i;
		dtrace_probedesc_t	desc;

		/*
		 * To avoid checking multiple times whether an element in the
		 * probe specification is a glob pattern, we (ab)use the
		 * desc->id value (unused at this point) to store this
		 * information a a bitmap.
		 */
		desc = *pdp;
		desc.id = (p_is_glob << 3) | (m_is_glob << 2) |
			  (f_is_glob << 1) | n_is_glob;

		for (i = 0; i < dtp->dt_probe_id; i++) {
			prp = dtp->dt_probes[i];
			if (prp && dt_probe_gmatch(prp, &desc))
				break;
		}
		if (i >= dtp->dt_probe_id)
			prp = NULL;
	}

	if (prp)
		return prp;

no_probe:
	dt_set_errno(dtp, EDT_NOPROBE);
	return NULL;
}

dt_probe_t *
dt_probe_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	return NULL;
}

void
dt_probe_delete(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	dt_htab_delete(dtp->dt_byprv, prp);
	dt_htab_delete(dtp->dt_bymod, prp);
	dt_htab_delete(dtp->dt_byfun, prp);
	dt_htab_delete(dtp->dt_byprb, prp);
	dt_htab_delete(dtp->dt_byfqn, prp);

	dtp->dt_probes[prp->desc->id] = NULL;

	/* FIXME: Add cleanup code for the dt_probe_t itself. */
}

static void
dt_probe_args_info(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	int			argc = 0;
	dt_argdesc_t		*argv = NULL;
	int			i, nc, xc;
	dtrace_typeinfo_t	dtt;

	if (!prp->prov->impl->probe_info)
		return;
	if (prp->prov->impl->probe_info(dtp, prp, &argc, &argv) < 0)
		return;

	if (!argc || !argv)
		return;

	nc = 0;
	for (xc = 0; xc < argc; xc++)
		nc = MAX(nc, argv[xc].mapping);
	nc++;				/* Number of nargs = highest arg + 1 */

	/*
	 * Now that we have discovered the number of native and translated
	 * arguments from the argument descriptions, allocate nodes for the
	 * arguments and their types.
	 */
	dt_probe_alloc_args(prp, nc, xc);

	if ((xc != 0 && prp->xargs == NULL) || (nc != 0 && prp->nargs == NULL))
		return;

	/*
	 * Iterate over the arguments and assign their types to prp->nargv[],
	 * prp->xargv[], and record mappings in prp->mapping[].
	 */
	for (i = 0; i < argc; i++) {
		if (dtrace_type_strcompile(dtp, argv[i].native, &dtt) != 0) {
			dt_dprintf("failed to resolve input type %s for "
				   "%s:%s:%s:%s arg #%d: %s\n", argv[i].native,
				   prp->desc->prv, prp->desc->mod,
				   prp->desc->fun, prp->desc->prb, i,
				   dtrace_errmsg(dtp, dtrace_errno(dtp)));

			dtt.dtt_object = NULL;
			dtt.dtt_ctfp = NULL;
			dtt.dtt_type = CTF_ERR;
		} else {
			dt_node_type_assign(prp->nargv[argv[i].mapping],
					    dtt.dtt_ctfp, dtt.dtt_type);
		}

		if (dtt.dtt_type != CTF_ERR &&
		    (!argv[i].xlate || strcmp(argv[i].native,
					      argv[i].xlate) == 0)) {
			dt_node_type_propagate(prp->nargv[argv[i].mapping],
					       prp->xargv[i]);
		} else if (dtrace_type_strcompile(dtp, argv[i].xlate,
						  &dtt) != 0) {
			dt_dprintf("failed to resolve output type %s for "
				   "%s:%s:%s:%s arg #%d: %s\n", argv[i].xlate,
				   prp->desc->prv, prp->desc->mod,
				   prp->desc->fun, prp->desc->prb, i,
				   dtrace_errmsg(dtp, dtrace_errno(dtp)));

			dtt.dtt_object = NULL;
			dtt.dtt_ctfp = NULL;
			dtt.dtt_type = CTF_ERR;
		} else {
			dt_node_type_assign(prp->xargv[i],
					    dtt.dtt_ctfp, dtt.dtt_type);
		}

		prp->mapping[i] = argv[i].mapping;
		prp->argv[i] = dtt;
	}

	dt_free(dtp, argv);
}

/*ARGSUSED*/
static int
dt_probe_first_match(dtrace_hdl_t *dtp, dt_probe_t *prp, dt_probe_t **prpp)
{
	if (*prpp == NULL) {
		*prpp = prp;
		return 0;
	}

	return 1;
}

dt_probe_t *
dt_probe_info(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
	      dtrace_probeinfo_t *pip)
{
	int p_is_glob = pdp->prv[0] == '\0' || strisglob(pdp->prv);
	int m_is_glob = pdp->mod[0] == '\0' || strisglob(pdp->mod);
	int f_is_glob = pdp->fun[0] == '\0' || strisglob(pdp->fun);
	int n_is_glob = pdp->prb[0] == '\0' || strisglob(pdp->prb);

	dt_probe_t *prp = NULL;
	const dtrace_pattr_t *pap;
	dt_provider_t *pvp;
	int m;

	/*
	 * Call dt_probe_iter() to find matching probes.  Our
	 * dt_probe_first_match() callback will produce the following results:
	 *
	 * m < 0 dt_probe_iter() found zero matches (or failed).
	 * m > 0 dt_probe_iter() found more than one match.
	 * m = 0 dt_probe_iter() found exactly one match.
	 */
	if ((m = dt_probe_iter(dtp, pdp, (dt_probe_f *)dt_probe_first_match, NULL, &prp)) < 0)
		return NULL; /* dt_errno is set for us */

	if ((pvp = prp->prov) == NULL)
		return NULL; /* dt_errno is set for us */

	/*
	 * If more than one probe was matched, then do not report probe
	 * information if either of the following conditions is true:
	 *
	 * (a) The Arguments Data stability of the matched provider is
	 *	less than Evolving.
	 *
	 * (b) Any description component that is at least Evolving is
	 *	empty or is specified using a globbing expression.
	 *
	 * These conditions imply that providers that provide Evolving
	 * or better Arguments Data stability must guarantee that all
	 * probes with identical field names in a field of Evolving or
	 * better Name stability have identical argument signatures.
	 */
	if (m > 0) {
		if (pvp->desc.dtvd_attr.dtpa_args.dtat_data <
		    DTRACE_STABILITY_EVOLVING && p_is_glob) {
			dt_set_errno(dtp, EDT_UNSTABLE);
			return NULL;
		}

		if (pvp->desc.dtvd_attr.dtpa_mod.dtat_name >=
		    DTRACE_STABILITY_EVOLVING && m_is_glob) {
			dt_set_errno(dtp, EDT_UNSTABLE);
			return NULL;
		}

		if (pvp->desc.dtvd_attr.dtpa_func.dtat_name >=
		    DTRACE_STABILITY_EVOLVING && f_is_glob) {
			dt_set_errno(dtp, EDT_UNSTABLE);
			return NULL;
		}

		if (pvp->desc.dtvd_attr.dtpa_name.dtat_name >=
		    DTRACE_STABILITY_EVOLVING && n_is_glob) {
			dt_set_errno(dtp, EDT_UNSTABLE);
			return NULL;
		}
	}

	/*
	 * Compute the probe description attributes by taking the minimum of
	 * the attributes of the specified fields.  If no provider is specified
	 * or a glob pattern is used for the provider, use Unstable attributes.
	 */
	if (p_is_glob)
		pap = &_dtrace_prvdesc;
	else
		pap = &pvp->desc.dtvd_attr;

	pip->dtp_attr = pap->dtpa_provider;

	if (!m_is_glob)
		pip->dtp_attr = dt_attr_min(pip->dtp_attr, pap->dtpa_mod);
	if (!f_is_glob)
		pip->dtp_attr = dt_attr_min(pip->dtp_attr, pap->dtpa_func);
	if (!n_is_glob)
		pip->dtp_attr = dt_attr_min(pip->dtp_attr, pap->dtpa_name);

	dt_probe_args_info(dtp, prp);

	pip->dtp_arga = pap->dtpa_args;
	pip->dtp_argv = prp->argv;
	pip->dtp_argc = prp->argc;

	return prp;
}

int
dtrace_probe_info(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
		  dtrace_probeinfo_t *pip)
{
	return dt_probe_info(dtp, pdp, pip) != NULL ? 0
						    : -1;
}

int
dt_probe_iter(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
	      dt_probe_f *pfunc, dtrace_probe_f *dfunc, void *arg)
{
	dtrace_probedesc_t	desc;
	dt_probe_t		tmpl;
	dt_probe_t		*prp;
	dt_provider_t		*pvp;
	int			i;
	int			p_is_glob, m_is_glob, f_is_glob, n_is_glob;
	int			rv = 0;
	int			matches = 0;

	assert(dfunc == NULL || pfunc == NULL);

	/*
	 * Special case: if no probe description is provided, we need to loop
	 * over all registered probes.
	 */
	if (!pdp) {
		for (i = 0; i < dtp->dt_probe_id; i++) {
			if (!dtp->dt_probes[i])
				continue;
			if (dfunc != NULL)
				rv = dfunc(dtp, dtp->dt_probes[i]->desc, arg);
			else if (pfunc != NULL)
				rv = pfunc(dtp, dtp->dt_probes[i], arg);

			if (rv != 0)
				return rv;

			matches++;
		}

		goto done;
	}

	/*
	 * Special case: If a probe id is provided, we can do a direct lookup.
	 */
	if (pdp->id != DTRACE_IDNONE) {
		if (pdp->id >= dtp->dt_probe_id)
			goto done;

		prp = dtp->dt_probes[pdp->id];
		if (!prp)
			goto done;

		if (dfunc != NULL)
			rv = dfunc(dtp, prp->desc, arg);
		else if (pfunc != NULL)
			rv = pfunc(dtp, prp, arg);

		if (rv != 0)
			return rv;

		matches = 1;
		goto done;
	}

	p_is_glob = pdp->prv[0] == '\0' || strisglob(pdp->prv);
	m_is_glob = pdp->mod[0] == '\0' || strisglob(pdp->mod);
	f_is_glob = pdp->fun[0] == '\0' || strisglob(pdp->fun);
	n_is_glob = pdp->prb[0] == '\0' || strisglob(pdp->prb);

	tmpl.desc = pdp;

	/*
	 * Loop over providers, allowing them to provide these probes.
	 */
	for (pvp = dt_list_next(&dtp->dt_provlist); pvp != NULL;
	     pvp = dt_list_next(pvp)) {
		if (pvp->impl->provide == NULL ||
		    !dt_gmatch(pvp->desc.dtvd_name, pdp->prv))
			continue;
		memcpy(&desc, pdp, sizeof(desc));
		desc.prv = pvp->desc.dtvd_name;
		pvp->impl->provide(dtp, &desc);
	}

	/*
	 * Special case: if the probe is fully specified (none of the elements
	 * are empty of a glob pattern, we can do a direct lookup based on the
	 * fully qualified probe name.
	 */
	if (p_is_glob + m_is_glob + f_is_glob + n_is_glob == 0) {
		prp = dt_htab_lookup(dtp->dt_byfqn, &tmpl);
		if (!prp)
			goto done;

		if (dfunc != NULL)
			rv = dfunc(dtp, prp->desc, arg);
		else if (pfunc != NULL)
			rv = pfunc(dtp, prp, arg);

		if (rv != 0)
			return rv;

		matches = 1;
		goto done;
	}

	/*
	 * If at least one probe name specification element is an exact string
	 * to match, use the most specific one to perform a htab lookup.  The
	 * order of preference is:
	 *	function name (usually best distribution of probes in htab)
	 *	probe name
	 *	module name
	 *	provider name (usually worst distribution of probes in htab)
	 *
	 * Further matching of probes will then continue based on other exact
	 * string elements, and finally glob-pattern elements.
	 *
	 * To avoid checking multiple times whether an element in the probe
	 * specification is a glob pattern, we (ab)use the desc->id value
	 * (unused at this point) to store this information a a bitmap.
	 */
	desc = *pdp;
	desc.id = (p_is_glob << 3) | (m_is_glob << 2) | (f_is_glob << 1) |
		  n_is_glob;

#define HTAB_GMATCH(c, nam)						\
	if (!c##_is_glob) {						\
		prp = dt_htab_lookup(dtp->dt_by##nam, &tmpl);		\
		if (!prp)						\
			goto done;					\
									\
		desc.nam = NULL;					\
		do {							\
			if (!dt_probe_gmatch(prp, &desc))		\
				continue;				\
									\
			if (dfunc != NULL)				\
				rv = dfunc(dtp, prp->desc, arg);	\
			else if (pfunc != NULL)				\
				rv = pfunc(dtp, prp, arg);		\
									\
			if (rv != 0)					\
				return rv;				\
									\
			matches++;					\
		} while ((prp = prp->he_##nam.next));			\
									\
		goto done;						\
	}

	HTAB_GMATCH(f, fun)
	HTAB_GMATCH(n, prb)
	HTAB_GMATCH(m, mod)
	HTAB_GMATCH(p, prv)

	/*
	 * If all probe specification elements are glob patterns, we have no
	 * choice but to run through the entire list of probes, matching them
	 * to the given probe description, one by one.
	 */
	for (i = 0; i < dtp->dt_probe_id; i++) {
		prp = dtp->dt_probes[i];
		if (!prp)
			continue;
		if (!dt_probe_gmatch(prp, &desc))
			continue;

		if (dfunc != NULL)
			rv = dfunc(dtp, prp->desc, arg);
		else if (pfunc != NULL)
			rv = pfunc(dtp, prp, arg);

		if (rv != 0)
			return rv;

		matches++;
	}

done:
	return matches ? 0
		       : dt_set_errno(dtp, EDT_NOPROBE);
}

int
dtrace_probe_iter(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
		  dtrace_probe_f *func, void *arg)
{
	return dt_probe_iter(dtp, pdp, NULL, func, arg);
}

int
dt_probe_add_clause(dtrace_hdl_t *dtp, dt_probe_t *prp, dt_ident_t *idp)
{
	dt_probeclause_t	*pcp;

	pcp = dt_zalloc(dtp, sizeof(dt_probeclause_t));;
	if (pcp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return -1;
	}

	pcp->clause = idp;
	dt_list_append(&prp->clauses, pcp);

	return 0;
}

int
dt_probe_clause_iter(dtrace_hdl_t *dtp, dt_probe_t *prp, dt_clause_f *func,
		     void *arg)
{
	dt_probeclause_t	*pcp;
	int			rc;

	assert(func != NULL);

	for (pcp = dt_list_next(&prp->clauses); pcp != NULL;
	     pcp = dt_list_next(pcp)) {
		rc = func(dtp, pcp->clause, arg);

		if (rc != 0)
			return rc;
	}

	return 0;
}

void
dt_probe_init(dtrace_hdl_t *dtp)
{
	dtp->dt_byprv = dt_htab_create(dtp, &prv_htab_ops);
	dtp->dt_bymod = dt_htab_create(dtp, &mod_htab_ops);
	dtp->dt_byfun = dt_htab_create(dtp, &fun_htab_ops);
	dtp->dt_byprb = dt_htab_create(dtp, &prb_htab_ops);
	dtp->dt_byfqn = dt_htab_create(dtp, &fqn_htab_ops);

	dtp->dt_probes = NULL;
	dtp->dt_probes_sz = 0;
	dtp->dt_probe_id = 1;
}

void
dt_probe_fini(dtrace_hdl_t *dtp)
{
	uint32_t	i;

	for (i = 0; i < dtp->dt_probes_sz; i++) {
		dt_probe_t	*prp = dtp->dt_probes[i];

		if (prp == NULL)
			continue;

		dt_probe_destroy(prp);
	}

	dt_htab_destroy(dtp, dtp->dt_byprv);
	dt_htab_destroy(dtp, dtp->dt_bymod);
	dt_htab_destroy(dtp, dtp->dt_byfun);
	dt_htab_destroy(dtp, dtp->dt_byprb);
	dt_htab_destroy(dtp, dtp->dt_byfqn);

	dt_free(dtp, dtp->dt_probes);
	dtp->dt_probes = NULL;
	dtp->dt_probes_sz = 0;
	dtp->dt_probe_id = 1;
}

void
dt_probe_stats(dtrace_hdl_t *dtp)
{
	dt_htab_stats("byprv", dtp->dt_byprv);
	dt_htab_stats("bymod", dtp->dt_bymod);
	dt_htab_stats("byfun", dtp->dt_byfun);
	dt_htab_stats("byprb", dtp->dt_byprb);
	dt_htab_stats("byfqn", dtp->dt_byfqn);
}

int
dtrace_id2desc(dtrace_hdl_t *dtp, dtrace_id_t id, dtrace_probedesc_t *pdp)
{
	dt_probe_t	*prp;

	if (id >= dtp->dt_probe_id)
		return dt_set_errno(dtp, EDT_NOPROBE);

	prp = dtp->dt_probes[id];
	if (!prp)
		return dt_set_errno(dtp, EDT_NOPROBE);

	memcpy(pdp, prp->desc, sizeof(dtrace_probedesc_t));

        return 0;
}
