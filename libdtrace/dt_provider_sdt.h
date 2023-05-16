/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROVIDER_SDT_H
#define	_DT_PROVIDER_SDT_H

#include <sys/dtrace_types.h>
#include <dt_provider.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Probe dependencies
 *
 * SDT probes are implemented using probes made available by other providers.
 * THe probe dependency table associates each SDT probe with one or more probe
 * specifications (possibly containing wildcards).  Each matching probe will
 * have SDT lockstat probe added as a dependent probe.
 */
typedef struct probe_dep {
	const char		*name;			/* probe name */
	dtrace_probespec_t	spec;			/* spec type */
	const char		*str;			/* spec string */
} probe_dep_t;

/*
 * Probe signature specifications
 *
 * This table *must* group the arguments of probes.  I.e. the arguments of a
 * given probe must be listed in consecutive records.
 *
 * A single probe entry that mentions only name of the probe indicates a probe
 * that provides no arguments.
 */
typedef struct probe_arg {
	const char	*name;			/* name of probe */
	int		argno;			/* argument number */
	dt_argdesc_t	argdesc;		/* argument description */
} probe_arg_t;

extern int dt_sdt_populate(dtrace_hdl_t *dtp, const char *prvname,
			   const char *modname, const dt_provimpl_t *ops,
			   const dtrace_pattr_t *pattr,
			   const probe_arg_t *probe_args,
			   const probe_dep_t *probes);
extern void dt_sdt_enable(dtrace_hdl_t *dtp, struct dt_probe *prp);
extern int dt_sdt_probe_info(dtrace_hdl_t *dtp, const struct dt_probe *prp,
			     int *argcp, dt_argdesc_t **argvp);
extern void dt_sdt_destroy(dtrace_hdl_t *dtp, void *datap);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROVIDER_SDT_H */
