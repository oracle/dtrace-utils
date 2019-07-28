/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROVIDER_H
#define	_DT_PROVIDER_H

#include <dt_impl.h>
#include <dt_ident.h>
#include <dt_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define TRACEFS		"/sys/kernel/debug/tracing/"
#define EVENTSFS	TRACEFS "events/"

typedef struct dt_provmod {
	char *name;			/* provider generic name */
	int (*populate)(void);		/* function to add probes */
} dt_provmod_t;

extern struct dt_provmod dt_dtrace;
extern struct dt_provmod dt_fbt;
extern struct dt_provmod dt_sdt;
extern struct dt_provmod dt_syscall;

typedef struct dt_provider {
	dt_list_t pv_list;		/* list forward/back pointers */
	struct dt_provider *pv_next;	/* pointer to next provider in hash */
	dtrace_providerdesc_t pv_desc;	/* provider name and attributes */
	dt_idhash_t *pv_probes;		/* probe defs (if user-declared) */
	dt_node_t *pv_nodes;		/* parse node allocation list */
	ulong_t *pv_xrefs;		/* translator reference bitmap */
	ulong_t pv_xrmax;		/* number of valid bits in pv_xrefs */
	ulong_t pv_gen;			/* generation # that created me */
	dtrace_hdl_t *pv_hdl;		/* pointer to containing dtrace_hdl */
	uint_t pv_flags;		/* flags (see below) */
} dt_provider_t;

#define	DT_PROVIDER_INTF	0x1	/* provider interface declaration */
#define	DT_PROVIDER_IMPL	0x2	/* provider implementation is loaded */

extern dt_provider_t *dt_provider_lookup(dtrace_hdl_t *, const char *);
extern dt_provider_t *dt_provider_create(dtrace_hdl_t *, const char *);
extern void dt_provider_destroy(dtrace_hdl_t *, dt_provider_t *);
extern int dt_provider_xref(dtrace_hdl_t *, dt_provider_t *, id_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROVIDER_H */
