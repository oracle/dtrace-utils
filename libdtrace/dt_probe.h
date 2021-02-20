/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROBE_H
#define	_DT_PROBE_H

#include <dt_impl.h>
#include <dt_ident.h>
#include <dt_list.h>
#include <dt_htab.h>
#include <dt_provider.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_probe_instance {
	char pi_fname[DTRACE_FUNCNAMELEN]; /* function name */
	char pi_rname[DTRACE_FUNCNAMELEN + 20]; /* mangled relocation name */
	uint32_t *pi_offs;		/* offsets into the function */
	uint32_t *pi_enoffs;		/* is-enabled offsets */
	uint_t pi_noffs;		/* number of offsets */
	uint_t pi_maxoffs;		/* size of pi_offs allocation */
	uint_t pi_nenoffs;		/* number of is-enabled offsets */
	uint_t pi_maxenoffs;		/* size of pi_enoffs allocation */
	struct dt_probe_instance *pi_next; /* next instance in the list */
} dt_probe_instance_t;

typedef struct dt_probe {
	dt_list_t list;			/* prev/next in enablings chain */
	dt_list_t clauses;		/* clauses to attach */
	const dtrace_probedesc_t *desc;	/* probe description (id, name) */
	dt_provider_t *prov;		/* pointer to containing provider */
	struct dt_hentry he_prv;	/* provider name htab links */
	struct dt_hentry he_mod;	/* module name htab links */
	struct dt_hentry he_fun;	/* function name htab links */
	struct dt_hentry he_prb;	/* probe name htab link */
	struct dt_hentry he_fqn;	/* fully qualified name htab link */
	void *prv_data;			/* provider-specific data */
	dt_ident_t *pr_ident;		/* pointer to probe identifier */
	const char *pr_name;		/* pointer to name component */
	dt_node_t *nargs;		/* native argument list */
	dt_node_t **nargv;		/* native argument vector */
	uint_t nargc;			/* native argument count */
	dt_node_t *xargs;		/* translated argument list */
	dt_node_t **xargv;		/* translated argument vector */
	uint_t xargc;			/* translated argument count */
	uint8_t *mapping;		/* translated argument mapping */
	dtrace_typeinfo_t *argv;	/* output argument types */
	int argc;			/* output argument count */
	dt_probe_instance_t *pr_inst;	/* list of functions and offsets */
} dt_probe_t;

extern dt_probe_t *dt_probe_lookup2(dt_provider_t *, const char *);
extern dt_probe_t *dt_probe_create(dtrace_hdl_t *, dt_ident_t *, int,
    dt_node_t *, uint_t, dt_node_t *, uint_t);

extern dt_probe_t *dt_probe_info(dtrace_hdl_t *,
    const dtrace_probedesc_t *, dtrace_probeinfo_t *);

extern void dt_probe_declare(dt_provider_t *, dt_probe_t *);
extern void dt_probe_destroy(dt_probe_t *);

extern int dt_probe_define(dt_provider_t *, dt_probe_t *,
    const char *, const char *, uint32_t, int);

extern dt_node_t *dt_probe_tag(dt_probe_t *, uint_t, dt_node_t *);

extern dt_probe_t *dt_probe_insert(dtrace_hdl_t *dtp, dt_provider_t *prov,
				     const char *prv, const char *mod,
				     const char *fun, const char *prb,
				     void *datap);
extern dt_probe_t *dt_probe_lookup(dtrace_hdl_t *dtp,
				   const dtrace_probedesc_t *pdp);
extern dt_probe_t *dt_probe_lookup_by_name(dtrace_hdl_t *dtp, const char *name);
extern void dt_probe_delete(dtrace_hdl_t *dtp, dt_probe_t *prp);

typedef int dt_probe_f(dtrace_hdl_t *dtp, dt_probe_t *prp, void *arg);
extern int dt_probe_iter(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp,
			 dt_probe_f *pfunc, dtrace_probe_f *dfunc, void *arg);

extern int dt_probe_add_clause(dtrace_hdl_t *dtp, dt_probe_t *prp,
			       dt_ident_t *idp);
typedef int dt_clause_f(dtrace_hdl_t *dtp, dt_ident_t *idp, void *arg);
extern int dt_probe_clause_iter(dtrace_hdl_t *dtp, dt_probe_t *prp,
				dt_clause_f *func, void *arg);


extern void dt_probe_init(dtrace_hdl_t *dtp);
extern void dt_probe_detach(dtrace_hdl_t *dtp);
extern void dt_probe_fini(dtrace_hdl_t *dtp);
extern void dt_probe_stats(dtrace_hdl_t *dtp);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROBE_H */
