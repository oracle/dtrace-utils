/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROVIDER_TP_H
#define	_DT_PROVIDER_TP_H

#include <dt_provider.h>

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

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROVIDER_TP_H */
