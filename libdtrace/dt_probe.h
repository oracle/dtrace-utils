/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROBE_H
#define	_DT_PROBE_H

#include <dt_impl.h>
#include <dt_ident.h>
#include <dt_list.h>
#include <dt_provider.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_probe_iter {
	dtrace_probedesc_t pit_desc;	/* description storage */
	dtrace_hdl_t *pit_hdl;		/* libdtrace handle */
	dt_provider_t *pit_pvp;		/* current provider */
	const char *pit_pat;		/* caller's name pattern (or NULL) */
	dtrace_probe_f *pit_func;	/* caller's function */
	void *pit_arg;			/* caller's argument */
	uint_t pit_matches;		/* number of matches */
} dt_probe_iter_t;

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
	dt_provider_t *pr_pvp;		/* pointer to containing provider */
	dt_ident_t *pr_ident;		/* pointer to probe identifier */
	const char *pr_name;		/* pointer to name component */
	dt_node_t *pr_nargs;		/* native argument list */
	dt_node_t **pr_nargv;		/* native argument vector */
	uint_t pr_nargc;		/* native argument count */
	dt_node_t *pr_xargs;		/* translated argument list */
	dt_node_t **pr_xargv;		/* translated argument vector */
	uint_t pr_xargc;		/* translated argument count */
	uint8_t *pr_mapping;		/* translated argument mapping */
	dt_probe_instance_t *pr_inst;	/* list of functions and offsets */
	dtrace_typeinfo_t *pr_argv;	/* output argument types */
	int pr_argc;			/* output argument count */
} dt_probe_t;

extern dt_provider_t *dt_provider_lookup(dtrace_hdl_t *, const char *);
extern dt_provider_t *dt_provider_create(dtrace_hdl_t *, const char *);
extern void dt_provider_destroy(dtrace_hdl_t *, dt_provider_t *);
extern int dt_provider_xref(dtrace_hdl_t *, dt_provider_t *, id_t);

extern dt_probe_t *dt_probe_create(dtrace_hdl_t *, dt_ident_t *, int,
    dt_node_t *, uint_t, dt_node_t *, uint_t);

extern dt_probe_t *dt_probe_info(dtrace_hdl_t *,
    const dtrace_probedesc_t *, dtrace_probeinfo_t *);

extern dt_probe_t *dt_probe_lookup(dt_provider_t *, const char *);
extern void dt_probe_declare(dt_provider_t *, dt_probe_t *);
extern void dt_probe_destroy(dt_probe_t *);

extern int dt_probe_define(dt_provider_t *, dt_probe_t *,
    const char *, const char *, uint32_t, int);

extern dt_node_t *dt_probe_tag(dt_probe_t *, uint_t, dt_node_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROBE_H */
