/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
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
 * PROBE_DATA provides the necessary data items to populate the format string.
 * PROBE_FMT formats that data.
 * PROBE_SFMT is a format string for recognizing PROBE_FMT data in sscanf().
 */
#define PROBE_DATA	getpid(), prp->desc->prv, prp->desc->prb
#define PROBE_FMT	"dt_%d_%s/%s"
#define PROBE_SFMT	"dt_%d_%ms"

typedef struct tp_probe tp_probe_t;

extern tp_probe_t *dt_tp_alloc(dtrace_hdl_t *dtp);
extern int dt_tp_attach(dtrace_hdl_t *dtp, tp_probe_t *tpp, int bpf_fd);
extern int dt_tp_attach_raw(dtrace_hdl_t *dtp, tp_probe_t *tpp,
			    const char *name, int bpf_fd);
extern int dt_tp_has_info(const tp_probe_t *tpp);
extern int dt_tp_event_info(dtrace_hdl_t *dtp, FILE *f, int skip,
			    tp_probe_t *tpp, int *argcp, dt_argdesc_t **argvp);
extern void dt_tp_detach(dtrace_hdl_t *dtp, tp_probe_t *tpp);
extern void dt_tp_destroy(dtrace_hdl_t *dtp, tp_probe_t *tpp);
extern uint32_t dt_tp_get_id(const tp_probe_t *prp);
extern void dt_tp_set_id(tp_probe_t *prp, uint32_t id);

extern struct dt_probe *dt_tp_probe_insert(dtrace_hdl_t *dtp,
					   dt_provider_t *prov,
					   const char *prv, const char *mod,
					   const char *fun, const char *prb);
extern int dt_tp_probe_info(dtrace_hdl_t *dtp, FILE *f, int skip,
			    const struct dt_probe *prp, int *argcp,
			    dt_argdesc_t **argvp);
extern int dt_tp_probe_has_info(const struct dt_probe *prp);
extern int dt_tp_probe_attach(dtrace_hdl_t *dtp, const struct dt_probe *prp,
			      int bpf_fd);
extern int dt_tp_probe_attach_raw(dtrace_hdl_t *dtp, const struct dt_probe *prp,
				  int bpf_fd);
extern void dt_tp_probe_detach(dtrace_hdl_t *dtp, const struct dt_probe *prp);
extern void dt_tp_probe_destroy(dtrace_hdl_t *dtp, void *datap);
extern uint32_t dt_tp_probe_get_id(const struct dt_probe *prp);
extern void dt_tp_probe_set_id(const struct dt_probe *prp, uint32_t id);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROVIDER_TP_H */
