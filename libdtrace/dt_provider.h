/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROVIDER_H
#define	_DT_PROVIDER_H

#include <dtrace/pid.h>
#include <dt_impl.h>
#include <dt_ident.h>
#include <dt_list.h>
#include <tracefs.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Tracepoint group naming format for DTrace providers.  Providers may append
 * to this format string as needed.
 *
 * GROUP_DATA provides the necessary data items to populate the format string
 * (PID of the dtrace process and the provider name).  GROUP_SFMT is like
 * GROUP_FMT, but for sscanf().
 */
#define GROUP_FMT	"dt_%d_%s"
#define GROUP_SFMT	"dt_%d_%ms"
#define GROUP_DATA	getpid(), prvname

struct dt_probe;

/*
 * Because it would waste both space and time, argument types are not recorded
 * with the probe by default.  In order to determine argument types for args[X]
 * variables, the D compiler queries for argument types on a probe-by-probe
 * basis.  (This optimizes for the common case that arguments are either not
 * used or used in an untyped fashion.)  Typed arguments are specified with a
 * string of the type name in the 'native' member of the argument description
 * structure.  Typed arguments may be further translated to types of greater
 * stability; the provider indicates such a translated argument by filling in
 * the 'xlate' member with the string of the translated type.
 * Finally, the provider may indicate which argument value a given argument
 * maps to by setting the 'mapping' member -- allowing a single argument to map
 * to multiple args[X] variables.
 */
typedef struct dt_argdesc {
#ifdef FIXME
	dtrace_id_t id;
	int ndx;
#endif
	int mapping;
	char *native;
	char *xlate;
} dt_argdesc_t;

typedef struct dt_provimpl {
	const char *name;			/* provider generic name */
	int prog_type;				/* BPF program type */
	int (*populate)(dtrace_hdl_t *dtp);	/* register probes */
	int (*provide)(dtrace_hdl_t *dtp,	/* provide probes */
		       const dtrace_probedesc_t *pdp);
	int (*provide_probe)(dtrace_hdl_t *dtp,	/* provide a specific probe */
		       const pid_probespec_t *psp);
	void (*enable)(dtrace_hdl_t *dtp,	/* enable the given probe */
		       struct dt_probe *prp);
	void (*trampoline)(dt_pcb_t *pcb);	/* generate BPF trampoline */
	int (*attach)(dtrace_hdl_t *dtp,	/* attach BPF prog to probe */
		      const struct dt_probe *prp, int bpf_fd);
	int (*probe_info)(dtrace_hdl_t *dtp,	/* get probe info */
			  const struct dt_probe *prp,
			  int *argcp, dt_argdesc_t **argvp);
	void (*detach)(dtrace_hdl_t *dtp,	/* probe cleanup */
		       const struct dt_probe *prb);
	void (*probe_destroy)(dtrace_hdl_t *dtp, /* free provider data */
			      void *datap);
	void *prv_data;				/* provider-specific data */
} dt_provimpl_t;

/* list dt_dtrace first */
extern dt_provimpl_t dt_dtrace;
extern dt_provimpl_t dt_cpc;
extern dt_provimpl_t dt_fbt;
extern dt_provimpl_t dt_profile;
extern dt_provimpl_t dt_sdt;
extern dt_provimpl_t dt_syscall;
extern dt_provimpl_t dt_uprobe;

typedef struct dt_provider {
	dt_list_t pv_list;		/* list forward/back pointers */
	struct dt_hentry he;		/* htab links */
	dtrace_providerdesc_t desc;	/* provider name and attributes */
	const dt_provimpl_t *impl;	/* provider implementation */
	dt_idhash_t *pv_probes;		/* probe defs (if user-declared) */
	dt_node_t *pv_nodes;		/* parse node allocation list */
	ulong_t *pv_xrefs;		/* translator reference bitmap */
	ulong_t pv_xrmax;		/* number of valid bits in pv_xrefs */
	ulong_t pv_gen;			/* generation # that created me */
	dtrace_hdl_t *pv_hdl;		/* pointer to containing dtrace_hdl */
	uint_t pv_flags;		/* flags (see below) */
} dt_provider_t;

typedef struct tp_probe tp_probe_t;

extern tp_probe_t *dt_tp_alloc(dtrace_hdl_t *dtp);
extern int dt_tp_attach(dtrace_hdl_t *dtp, tp_probe_t *tpp, int bpf_fd);
extern int dt_tp_is_created(const tp_probe_t *tpp);
extern int dt_tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip,
			    tp_probe_t *tpp, int *argcp,
			    dt_argdesc_t **argvp);
extern void dt_tp_detach(dtrace_hdl_t *dtp, tp_probe_t *tpp);
extern void dt_tp_destroy(dtrace_hdl_t *dtp, tp_probe_t *tpp);

extern struct dt_probe *dt_tp_probe_insert(dtrace_hdl_t *dtp,
					   dt_provider_t *prov,
					   const char *prv, const char *mod,
					   const char *fun, const char *prb);
extern int dt_tp_probe_attach(dtrace_hdl_t *dtp, const struct dt_probe *prp,
			      int bpf_fd);
extern int dt_tp_probe_attach_raw(dtrace_hdl_t *dtp, const struct dt_probe *prp,
				  int bpf_fd);
extern void dt_tp_probe_detach(dtrace_hdl_t *dtp, const struct dt_probe *prp);
extern void dt_tp_probe_destroy(dtrace_hdl_t *dtp, void *datap);

#define	DT_PROVIDER_INTF	0x1	/* provider interface declaration */
#define	DT_PROVIDER_IMPL	0x2	/* provider implementation is loaded */
#define	DT_PROVIDER_PID		0x4	/* provider is a PID provider */

extern dt_provider_t *dt_provider_lookup(dtrace_hdl_t *, const char *);
extern dt_provider_t *dt_provider_create(dtrace_hdl_t *, const char *,
					 const dt_provimpl_t *,
					 const dtrace_pattr_t *);
extern int dt_provider_xref(dtrace_hdl_t *, dt_provider_t *, id_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROVIDER_H */
