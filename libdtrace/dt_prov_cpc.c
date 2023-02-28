/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The CPU Performance Counter (CPC) provider for DTrace.
 */
#include <assert.h>
#include <dt_impl.h>
#include <sys/ioctl.h>
#include <ctype.h>				/* tolower() */

#include <bpf_asm.h>
#include <linux/perf_event.h>
#include <perfmon/pfmlib_perf_event.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_bpf.h"
#include "dt_probe.h"

static const char		prvname[] = "cpc";
static const char		modname[] = "";
static const char		funname[] = "";

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

typedef struct cpc_probe {
	char	*name;
	int	*fds;
} cpc_probe_t;

/*
 * Probe name mappings.  As we discover which events can actually be used
 * on the system, we put them in a linked list that maps from names
 * we present to the D/CPC user to names used by the underlying system.
 * Importantly, CPC wants no '-' in probe names.  And, stylistically, we
 * prefer lower-case probe names.
 */
typedef struct cpc_probe_map {
	dt_list_t		list;
	char			*Dname;
	char			*pfmname;
} cpc_probe_map_t;

static dt_probe_t *cpc_probe_insert(dtrace_hdl_t *dtp, const char *prb)
{
	dt_provider_t	*prv;
	cpc_probe_t	*datap;
	int		i, cnt = dtp->dt_conf.num_online_cpus;

	prv = dt_provider_lookup(dtp, prvname);
	if (!prv)
		return 0;

	datap = dt_zalloc(dtp, sizeof(cpc_probe_t));
	if (datap == NULL)
		return NULL;

	datap->name = strdup(prb);
	datap->fds = dt_calloc(dtp, cnt, sizeof(int));
	if (datap->fds == NULL)
		goto err;

	for (i = 0; i < cnt; i++)
		datap->fds[i] = -1;

	return dt_probe_insert(dtp, prv, prvname, modname, funname, prb, datap);

err:
	dt_free(dtp, datap);
	return NULL;
}

static int populate(dtrace_hdl_t *dtp)
{
	int		n = 0;

	dt_provider_create(dtp, prvname, &dt_cpc, &pattr);
	dt_cpc.prv_data = dt_zalloc(dtp, sizeof(dt_list_t));

	/* incidentally, pfm_strerror(pfm_initialize()) describes the error */
	if (pfm_initialize() != PFM_SUCCESS)
		return 0;

	/* loop over PMUs (FWIW, ipmu=PFM_PMU_PERF_EVENT is among them) */
	for (pfm_pmu_t ipmu = PFM_PMU_NONE; ipmu < PFM_PMU_MAX; ipmu ++) {
		pfm_pmu_info_t pmuinfo;

		memset(&pmuinfo, 0, sizeof(pmuinfo));
		pmuinfo.size = sizeof(pfm_pmu_info_t);
		if (pfm_get_pmu_info(ipmu , &pmuinfo) != PFM_SUCCESS || pmuinfo.is_present == 0)
			continue;

		/*
		 * At this point, we have interesting information like:
		 *     - pmuinfo.nevents
		 *     - pmuinfo.name
		 *     - pmuinfo.desc
		 *     - pmuinfo.type = PFM_PMU_TYPE_[UNKNOWN|CORE|UNCORE]
		 *     - pmuinfo.num_cntrs
		 *     - pmuinfo.fixed_num_cntrs
		 *     - pmuinfo.max_encoding - number of event codes returned by pfm_get_event_encoding()
		 */

		/* loop over events */
		for (int ievt = pmuinfo.first_event; ievt != -1; ievt = pfm_get_event_next(ievt)) {
			pfm_event_info_t evtinfo;

			pfm_perf_encode_arg_t encoding;
			struct perf_event_attr attr;
			char *fstr = NULL;

			int fd;

			cpc_probe_map_t *next_probe_map;

			char *suffix = "-all-1000000000";
			char *s;

			dtrace_probedesc_t pd;

			/*
			 * Convert opaque integer index ievt into a name evt.name.
			 */

			memset(&evtinfo, 0, sizeof(evtinfo));
			evtinfo.size = sizeof(evtinfo);

			/* PFM_OS_[NONE|PERF_EVENT|PERF_EVENT_EXT] */
			if (pfm_get_event_info(ievt, PFM_OS_PERF_EVENT, &evtinfo) != PFM_SUCCESS)
				continue;

			/*
			 * At this point, we have interesting information like:
			 *     - evtinfo.name
			 *     - evtinfo.desc   - a little verbose and does not say that much
			 *     - evtinfo.nattrs
			 *     - evtinfo.dtype  - should be PFM_DTYPE_UINT64
			 *     - evtinfo.idx    - should be ievt
			 *     - evtinfo.equiv  - some equivalent name, or "(null)"
			 */

			/*
			 * Convert the event name into perf_event attr.
			 */
			memset(&encoding, 0, sizeof(encoding));
			memset(&attr, 0, sizeof(attr));
			encoding.size = sizeof(encoding);
			encoding.attr = &attr;
			encoding.fstr = &fstr;

			/*
			 * os = [PFM_OS_PERF_EVENT | PFM_OS_PERF_EVENT_EXT]
			 * Note that pfm_strerror(pfm_get_os_event_encoding(...)) describes any error.
			 */
			if (pfm_get_os_event_encoding(evtinfo.name, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &encoding) != PFM_SUCCESS) {
				if (fstr)
					free(fstr);    /* is this necessary if we errored out? */
				continue;
			}

			/*
			 * At this point, ievt is what we requested, while encoding.idx corresponds to fstr.
			 * Meanwhile, fstr will have some ":u=1:k=1" that we would otherwise want to modify.
			 */
			if (fstr)
				free(fstr);

			/*
			 * Now attr is largely set up.  Note:
			 *     - attr.size is still 0, which is okay
			 *     - attr.freq is 0, which is okay
			 *     - attr.wakeup_events is 0, which we can change
			 */
			attr.wakeup_events = 1;

			/*
			 * Check attr with perf_event_open().
			 */
			fd = perf_event_open(&attr, -1, 0 /* FIXME: cpu */, -1, 0);
			if (fd < 0)
				continue;
			close(fd);

			/*
			 * We convert '-' to '_' to conform to CPC practices
			 * and convert to lower-case characters (for stylistic reasons).
			 *
			 * FIXME: If we run out of memory (which is unlikely?), we can:
			 *   - just proceed with the NULL pointers (causing later drastic failure)
			 *   - silently skip over this probe (causing later more controlled failure)
			 *   - somehow emit a diagnostic message
			 * For now, we just choose the middle option.
			 *
			 * FIXME: Memory pointed to by next_probe_map, pfmname, and Dname
			 * should ideally be freed explicitly during some probe_destroy(),
			 * but this is a low priority since all such memory will be freed
			 * anyhow when the DTrace session ends.
			 */
			next_probe_map = dt_zalloc(dtp, sizeof(cpc_probe_map_t));
			if (next_probe_map == NULL)
				continue;
			next_probe_map->pfmname = strdup(evtinfo.name);
			next_probe_map->Dname = strdup(evtinfo.name);
			if (next_probe_map->pfmname == NULL ||
			    next_probe_map->Dname == NULL)
				continue;
			for (unsigned char *p = next_probe_map->Dname; *p; p++)
				*p = (*p == '-') ? '_' : tolower(*p);
			dt_list_append(dt_cpc.prv_data, next_probe_map);

			/*
			 * Compose a CPC probe name by adding mode "all" and a sample period
			 * big enough that even the fastest firing probe will not be unreasonable.
			 */
			s = dt_zalloc(dtp, strlen(next_probe_map->Dname) + strlen(suffix) + 1);
			if (s == NULL)
				continue;
			sprintf(s, "%s%s", next_probe_map->Dname, suffix);

			/*
			 * If this probe is not yet there (likely!), add it.
			 */
			pd.id = DTRACE_IDNONE;
			pd.prv = prvname;
			pd.mod = modname;
			pd.fun = funname;
			pd.prb = s;
			if (dt_probe_lookup(dtp, &pd) == NULL && cpc_probe_insert(dtp, s))
				n++;

			dt_free(dtp, s);
		}
	}

	return n;
}

static int decode_event(struct perf_event_attr *ap, const char *name) {
	cpc_probe_map_t *probe_map;
	pfm_perf_encode_arg_t encoding;

	/* find the probe name mapping for this D name */
	for (probe_map = dt_list_next(dt_cpc.prv_data);
	    probe_map; probe_map = dt_list_next(probe_map))
		if (strcmp(name, probe_map->Dname) == 0)
			break;
	if (probe_map == NULL)
		return -1;

	/* fill in the attr for this pfm name */
	char *fstr = NULL;
	int ret;

	memset(&encoding, 0, sizeof(encoding));
	encoding.size = sizeof(encoding);
	encoding.attr = ap;
	encoding.fstr = &fstr;

	/*
	 * os = [PFM_OS_PERF_EVENT | PFM_OS_PERF_EVENT_EXT]
	 * Note that pfm_strerror(pfm_get_os_event_encoding(...)) describes any error.
	 */
	ret = pfm_get_os_event_encoding(probe_map->pfmname, PFM_PLM0 | PFM_PLM3, PFM_OS_PERF_EVENT, &encoding);
	if (fstr)
		free(fstr);    /* FIXME: is this necessary if we errored out?  if not, we do not need to define ret? */
	return (ret == PFM_SUCCESS) ? 0 : -1;
}

static int decode_mode(struct perf_event_attr *ap, const char *name) {
	if (strcmp(name, "user") == 0) {
		ap->exclude_kernel = 1;
		return 0;
	} else if (strcmp(name, "kernel") == 0) {
		ap->exclude_user = 1;
		return 0;
	} else if (strcmp(name, "all") == 0)
		return 0;

	return -1;
}

static int decode_attributes(struct perf_event_attr *ap, const char *name) {
	/* FIXME: need to implement this */
	return -1;
}

static int decode_probename(struct perf_event_attr *ap, const char *name) {
	char buf[DTRACE_NAMELEN];
	char *pend;

	/* work in a temporary space */
	strcpy(buf, name);

	/* "event" substring */
	name = buf;
	pend = strchr(name, '-');
	if (pend == NULL)
		return -1;
	*pend = '\0';
	pend++;
	if (decode_event(ap, name) < 0)
		return -1;

	/* "mode" substring */
	name = pend;
	pend = strchr(name, '-');
	if (pend == NULL)
		return -1;
	*pend = '\0';
	pend++;
	if (decode_mode(ap, name) < 0)
		return -1;

	/* optional "attributes" substring */
	name = pend;
	pend = strchr(name, '-');
	if (pend) {
		*pend = '\0';
		pend++;
		if (decode_attributes(ap, name) < 0)
			return -1;
		name = pend;
	}

	/* "count" substring must be all digits 0-9 */
	if (strspn(name, "0123456789") < strlen(name))
		return -1;
	if (sscanf(name, "%llu", &ap->sample_period) != 1)
		return -1;

	return 0;
}

static int provide(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	struct perf_event_attr attr;

	/* make sure we have IDNONE and a legal name */
	if (pdp->id != DTRACE_IDNONE || strcmp(pdp->prv, prvname) ||
	    strcmp(pdp->mod, modname) || strcmp(pdp->fun, funname))
		return 0;

	/* return if we already have this probe */
	if (dt_probe_lookup(dtp, pdp))
		return 0;

	/* check if the probe name can be decoded */
	if (decode_probename(&attr, pdp->prb) == -1)
		return 0;

	/* try to add this probe */
	if (cpc_probe_insert(dtp, pdp->prb) == NULL)
		return 0;

	return 1;
}

/*
 * Generate a BPF trampoline for a cpc probe.
 *
 * The trampoline function is called when a cpc probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_cpc(struct bpf_perf_event_data *ctx)
 *
 * The trampoline will populate a dt_bpf_context struct and then call the
 * function that implements the compiled D clause.  It returns the value that
 * it gets back from that function.
 *
 * The context that is passed to the trampoline is:
 *     struct bpf_perf_event_data {
 *         bpf_user_pt_regs_t regs;
 *         __u64 sample_period;
 *         __u64 addr;
 *     }
 */
static void trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_copy_regs(pcb);

	/*
	 * Use the PC to set arg0 and arg1, then clear the other args.
	 */
	dt_cg_tramp_copy_pc_from_regs(pcb);
	for (i = 2; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));

	dt_cg_tramp_epilogue(pcb);
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	cpc_probe_t		*datap = prp->prv_data;
	struct perf_event_attr	attr;
	int			i, nattach = 0;;
	int			cnt = dtp->dt_conf.num_online_cpus;
	char			*name = datap->name;  /* same as prp->desc->prb */

	memset(&attr, 0, sizeof(attr));
	if (decode_probename(&attr, name) < 0)
		return -1;
	attr.wakeup_events = 1;

	for (i = 0; i < cnt; i++) {
		int fd;

		fd = perf_event_open(&attr, -1, dtp->dt_conf.cpus[i].cpu_id,
				     -1, 0);
		if (fd < 0)
			continue;
		if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0) {
			close(fd);
			continue;
		}
		datap->fds[i] = fd;
		nattach++;
	}

	return nattach > 0 ? 0 : -1;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	/* cpc-provider probe arguments are not typed */
	*argcp = 0;
	*argvp = NULL;

	return 0;
}

static void detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	cpc_probe_t	*datap = prp->prv_data;
	int		i, cnt = dtp->dt_conf.num_online_cpus;

	for (i = 0; i < cnt; i++) {
		if (datap->fds[i] != -1)
			close(datap->fds[i]);
	}
}

static void probe_destroy(dtrace_hdl_t *dtp, void *arg)
{
	cpc_probe_t	*datap = arg;

	dt_free(dtp, datap->fds);
	dt_free(dtp, datap->name);
	dt_free(dtp, datap);
}

dt_provimpl_t	dt_cpc = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_PERF_EVENT,
	.populate	= &populate,
	.provide	= &provide,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.probe_info	= &probe_info,
	.detach		= &detach,
	.probe_destroy	= &probe_destroy,
};
