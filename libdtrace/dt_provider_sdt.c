/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Provider support code for tracepoint-based probes.
 */
#include <errno.h>
#include <ctype.h>

#include "dt_provider_sdt.h"
#include "dt_probe.h"
#include "dt_impl.h"

typedef struct sdt_data {
	const probe_dep_t	*probes;
	const probe_arg_t	*probe_args;
} sdt_data_t;

/*
 * Create an SDT provider and populate it from the provided list of probes
 * (with dependency information) and probe argument data.
 */
int
dt_sdt_populate(dtrace_hdl_t *dtp, const char *prvname, const char *modname,
		const dt_provimpl_t *ops, const dtrace_pattr_t *pattr,
		const probe_arg_t *probe_args, const probe_dep_t *probes)
{
	dt_provider_t		*prv;
	const probe_arg_t	*arg;
	sdt_data_t		*sdp;
	int			n = 0;

	sdp = dt_alloc(dtp, sizeof(sdt_data_t));
	if (sdp == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	sdp->probes = probes;
	sdp->probe_args = probe_args;
	prv = dt_provider_create(dtp, prvname, ops, pattr, sdp);
	if (prv == NULL)
		return -1;			/* errno already set */

	/*
	 * Create SDT probes based on the probe_args list.  Since each probe
	 * will have at least one entry (with argno == 0), we can use those
	 * entries to identify the probe names.
	 */
	for (arg = &probe_args[0]; arg->name != NULL; arg++) {
		if (arg->argno == 0 &&
		    dt_probe_insert(dtp, prv, prvname, modname, "", arg->name,
				    NULL))
			n++;
	}

	return n;
}

/*
 * Add an SDT probe as a dependent probe for an underlying probe that has been
 * provided by another provider.
 */
static int
add_dependency(dtrace_hdl_t *dtp, dt_probe_t *uprp, void *arg)
{
	dt_probe_t	*prp = arg;

	dt_probe_add_dependent(dtp, uprp, prp);
	dt_probe_enable(dtp, uprp);

	return 0;
}

/*
 * Enable an SDT probe.
 */
void
dt_sdt_enable(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	const probe_dep_t	*dep;

	for (dep = ((sdt_data_t *)prp->prov->prv_data)->probes;
	     dep->name != NULL; dep++) {
		dtrace_probedesc_t	pd;

		if (strcmp(prp->desc->prb, dep->name) != 0)
			continue;

		/*
		 * If the dependency specifies a minimum and/or maximum kernel
		 * version, skip this dependency if the runtime kernel version
		 * does not fall in the specified range.
		 */
		if (dep->kver_min && dtp->dt_kernver < dep->kver_min)
			continue;
		if (dep->kver_max && dtp->dt_kernver > dep->kver_max)
			continue;

		if (dtrace_str2desc(dtp, dep->spec, dep->str, &pd) == -1)
			return;

		dt_probe_iter(dtp, &pd, add_dependency, NULL, prp);

		free((void *)pd.prv);
		free((void *)pd.mod);
		free((void *)pd.fun);
		free((void *)pd.prb);
	}

	/*
	 * Finally, ensure we're in the list of enablings as well.
	 * (This ensures that, among other things, the probes map
	 * gains entries for us.)
	 */
	if (!dt_in_list(&dtp->dt_enablings, prp))
		dt_list_append(&dtp->dt_enablings, prp);
}

/*
 * Populate the probe arguments for the given probe.
 */
int
dt_sdt_probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp, int *argcp,
		  dt_argdesc_t **argvp)
{
	int			i;
	int			pidx = -1;
	int			argc = 0;
	dt_argdesc_t		*argv = NULL;
	const probe_arg_t	*probe_args =
				((sdt_data_t *)prp->prov->prv_data)->probe_args;
	const probe_arg_t	*arg;

	for (arg = &probe_args[i = 0]; arg->name != NULL; arg++, i++) {
		if (strcmp(arg->name, prp->desc->prb) == 0) {
			if (pidx == -1) {
				pidx = i;

				if (arg->argdesc.native == NULL)
					break;
			}

			argc++;
		}
	}

	if (argc == 0)
		goto done;

	argv = dt_zalloc(dtp, argc * sizeof(dt_argdesc_t));
	if (!argv)
		return -ENOMEM;

	for (i = pidx; i < pidx + argc; i++) {
		const probe_arg_t	*arg = &probe_args[i];
		const dt_argdesc_t	*argd = &arg->argdesc;
		dt_argdesc_t		*parg = &argv[arg->argno];

		*parg = *argd;
		if (argd->native)
			parg->native = strdup(argd->native);
		if (argd->xlate)
			parg->xlate = strdup(argd->xlate);
	}

done:
	*argcp = argc;
	*argvp = argv;

	return 0;
}

/*
 * Clean up SDT-specific data.
 */
void
dt_sdt_destroy(dtrace_hdl_t *dtp, void *datap)
{
	dt_free(dtp, datap);
}
