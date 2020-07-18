/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <dt_impl.h>
#include <dt_pcb.h>
#include <dt_probe.h>
#include <dt_printf.h>

static void
dt_datadesc_hold(dtrace_datadesc_t *ddp)
{
	ddp->dtdd_refcnt++;
}

void
dt_datadesc_release(dtrace_hdl_t *dtp, dtrace_datadesc_t *ddp)
{
	int			i;
	dtrace_recdesc_t	*rec;

	if (--ddp->dtdd_refcnt > 0)
		return;

	for (i = 0, rec = &ddp->dtdd_recs[0]; i < ddp->dtdd_nrecs; i++, rec++) {
		if (rec->dtrd_format != NULL)
			dt_printf_destroy(rec->dtrd_format);
	}
	dt_free(dtp, ddp->dtdd_recs);
	dt_free(dtp, ddp);
}

dtrace_datadesc_t *
dt_datadesc_create(dtrace_hdl_t *dtp)
{
	dtrace_datadesc_t	*ddp;

	ddp = dt_zalloc(dtp, sizeof(dtrace_datadesc_t));
	if (ddp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	dt_datadesc_hold(ddp);

	return ddp;
}

int
dt_datadesc_finalize(dtrace_hdl_t *dtp, dtrace_datadesc_t *ddp)
{
	dt_pcb_t		*pcb = dtp->dt_pcb;
	dtrace_datadesc_t	*oddp = pcb->pcb_ddesc;

	/*
	 * If the number of allocated data records is greater than the actual
	 * number needed, we create a copy with the right number of records and
	 * free the oversized one.
	 */
	if (oddp->dtdd_nrecs < pcb->pcb_maxrecs) {
		dtrace_recdesc_t	*nrecs;

		nrecs = dt_calloc(dtp, oddp->dtdd_nrecs,
				  sizeof(dtrace_recdesc_t));
		if (nrecs == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);

		memcpy(nrecs, ddp->dtdd_recs,
		       oddp->dtdd_nrecs * sizeof(dtrace_recdesc_t));
		dt_free(dtp, ddp->dtdd_recs);
		ddp->dtdd_recs = nrecs;
		pcb->pcb_maxrecs = oddp->dtdd_nrecs;
	}

	ddp->dtdd_nrecs = oddp->dtdd_nrecs;

	return 0;
}

/*
 * Associate a probe data description and probe description with an enabled
 * probe ID.  This means that the given ID refers to the program matching the
 * probe data description being attached to the probe that matches the probe
 * description.
 */
dtrace_epid_t
dt_epid_add(dtrace_hdl_t *dtp, dtrace_datadesc_t *ddp, dtrace_id_t prid)
{
	dtrace_id_t		max = dtp->dt_maxprobe;
	dtrace_epid_t		epid;

	epid = dtp->dt_nextepid++;
	if (epid >= max || dtp->dt_ddesc == NULL) {
		dtrace_id_t		nmax = max ? (max << 1) : 2;
		dtrace_datadesc_t	**nddesc;
		dtrace_probedesc_t	**npdesc;

		nddesc = dt_calloc(dtp, nmax, sizeof(void *));
		npdesc = dt_calloc(dtp, nmax, sizeof(void *));
		if (nddesc == NULL || npdesc == NULL) {
			dt_free(dtp, nddesc);
			dt_free(dtp, npdesc);
			return dt_set_errno(dtp, EDT_NOMEM);
		}

		if (dtp->dt_ddesc != NULL) {
			size_t	osize = max * sizeof(void *);

			memcpy(nddesc, dtp->dt_ddesc, osize);
			dt_free(dtp, dtp->dt_ddesc);
			memcpy(npdesc, dtp->dt_pdesc, osize);
			dt_free(dtp, dtp->dt_pdesc);
		}

		dtp->dt_ddesc = nddesc;
		dtp->dt_pdesc = npdesc;
		dtp->dt_maxprobe = nmax;
	}

	if (dtp->dt_ddesc[epid] != NULL)
		return epid;

	dt_datadesc_hold(ddp);
	dtp->dt_ddesc[epid] = ddp;
	dtp->dt_pdesc[epid] = (dtrace_probedesc_t *)dtp->dt_probes[prid]->desc;

	return epid;
}

int
dt_epid_lookup(dtrace_hdl_t *dtp, dtrace_epid_t epid, dtrace_datadesc_t **ddp,
	       dtrace_probedesc_t **pdp)
{
	assert(epid < dtp->dt_maxprobe);
	assert(dtp->dt_ddesc[epid] != NULL);
	assert(dtp->dt_pdesc[epid] != NULL);

	*ddp = dtp->dt_ddesc[epid];
	*pdp = dtp->dt_pdesc[epid];

	return (0);
}

void
dt_epid_destroy(dtrace_hdl_t *dtp)
{
	size_t i;

	assert((dtp->dt_pdesc != NULL && dtp->dt_ddesc != NULL &&
	    dtp->dt_maxprobe > 0) || (dtp->dt_pdesc == NULL &&
	    dtp->dt_ddesc == NULL && dtp->dt_maxprobe == 0));

	if (dtp->dt_pdesc == NULL)
		return;

	for (i = 0; i < dtp->dt_maxprobe; i++) {
		if (dtp->dt_ddesc[i] == NULL) {
			assert(dtp->dt_pdesc[i] == NULL);
			continue;
		}

		dt_datadesc_release(dtp, dtp->dt_ddesc[i]);
		assert(dtp->dt_pdesc[i] != NULL);
	}

	free(dtp->dt_pdesc);
	dtp->dt_pdesc = NULL;

	free(dtp->dt_ddesc);
	dtp->dt_ddesc = NULL;
	dtp->dt_nextepid = 0;
	dtp->dt_maxprobe = 0;
}

uint32_t
dt_rec_add(dtrace_hdl_t *dtp, dt_cg_gap_f gapf, dtrace_actkind_t kind,
	   uint32_t size, uint16_t alignment, dt_pfargv_t *pfp, uint64_t arg)
{
	dt_pcb_t		*pcb = dtp->dt_pcb;
	uint32_t		off;
	uint32_t		gap;
	dtrace_datadesc_t	*ddp = pcb->pcb_ddesc;
	dtrace_recdesc_t	*rec;
	int			cnt, max;

	assert(gapf);

	cnt = ddp->dtdd_nrecs + 1;
	max = pcb->pcb_maxrecs;
	if (cnt >= max) {
		int			nmax = max ? (max << 1) : cnt;
		dtrace_recdesc_t	*nrecs;

		nrecs = dt_calloc(dtp, nmax, sizeof(dtrace_recdesc_t));
		if (nrecs == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);

		if (ddp->dtdd_recs != NULL) {
			size_t	osize = max * sizeof(dtrace_recdesc_t);

			memcpy(nrecs, ddp->dtdd_recs, osize);
			dt_free(dtp, ddp->dtdd_recs);
		}

		ddp->dtdd_recs = nrecs;
		pcb->pcb_maxrecs = nmax;
	}

	rec = &ddp->dtdd_recs[ddp->dtdd_nrecs++];
	off = (pcb->pcb_bufoff + (alignment - 1)) & ~(alignment - 1);
	rec->dtrd_action = kind;
	rec->dtrd_size = size;
	rec->dtrd_offset = off;
	rec->dtrd_alignment = alignment;
	rec->dtrd_format = pfp;
	rec->dtrd_arg = arg;

	gap = off - pcb->pcb_bufoff;
	if (gap > 0)
		(*gapf)(pcb, gap);

	pcb->pcb_bufoff = off + size;

	return off;
}

static int
dt_aggid_add(dtrace_hdl_t *dtp, dtrace_aggid_t id)
{
	dtrace_id_t max;
	int rval;

	while (id >= (max = dtp->dt_maxagg) || dtp->dt_aggdesc == NULL) {
		dtrace_id_t new_max = max ? (max << 1) : 1;
		size_t nsize = new_max * sizeof (void *);
		dtrace_aggdesc_t **new_aggdesc;

		if ((new_aggdesc = malloc(nsize)) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));

		memset(new_aggdesc, 0, nsize);

		if (dtp->dt_aggdesc != NULL) {
			memcpy(new_aggdesc, dtp->dt_aggdesc,
			    max * sizeof (void *));
			free(dtp->dt_aggdesc);
		}

		dtp->dt_aggdesc = new_aggdesc;
		dtp->dt_maxagg = new_max;
	}

	if (dtp->dt_aggdesc[id] == NULL) {
		dtrace_aggdesc_t *agg, *nagg;

		if ((agg = malloc(sizeof (dtrace_aggdesc_t))) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));

		memset(agg, 0, sizeof (dtrace_aggdesc_t));
		agg->dtagd_id = id;
		agg->dtagd_nrecs = 1;

		if (dt_ioctl(dtp, DTRACEIOC_AGGDESC, agg) == -1) {
			rval = dt_set_errno(dtp, errno);
			free(agg);
			return (rval);
		}

		if (DTRACE_SIZEOF_AGGDESC(agg) != sizeof (*agg)) {
			/*
			 * There must be more than one action.  Allocate the
			 * appropriate amount of space and try again.
			 */
			if ((nagg = malloc(DTRACE_SIZEOF_AGGDESC(agg))) != NULL)
				memcpy(nagg, agg, sizeof (*agg));

			free(agg);

			if ((agg = nagg) == NULL)
				return (dt_set_errno(dtp, EDT_NOMEM));

			rval = dt_ioctl(dtp, DTRACEIOC_AGGDESC, agg);

			if (rval == -1) {
				rval = dt_set_errno(dtp, errno);
				free(agg);
				return (rval);
			}
		}

		/*
		 * If we have a uarg, it's a pointer to the compiler-generated
		 * statement; we'll use this value to get the name and
		 * compiler-generated variable ID for the aggregation.  If
		 * we're grabbing an anonymous enabling, this pointer value
		 * is obviously meaningless -- and in this case, we can't
		 * provide the compiler-generated aggregation information.
		 */
		if (dtp->dt_options[DTRACEOPT_GRABANON] == DTRACEOPT_UNSET &&
		    agg->dtagd_rec[0].dtrd_uarg != 0) {
			dtrace_stmtdesc_t *sdp;
			dt_ident_t *aid;

			sdp = (dtrace_stmtdesc_t *)(uintptr_t)
			    agg->dtagd_rec[0].dtrd_uarg;
			aid = sdp->dtsd_aggdata;
			agg->dtagd_name = aid->di_name;
			agg->dtagd_varid = aid->di_id;
		} else {
			agg->dtagd_varid = DTRACE_AGGVARIDNONE;
		}

#if 0
		if ((epid = agg->dtagd_epid) >= dtp->dt_maxprobe ||
		    dtp->dt_pdesc[epid] == NULL) {
			if ((rval = dt_epid_add(dtp, epid)) != 0) {
				free(agg);
				return (rval);
			}
		}
#endif

		dtp->dt_aggdesc[id] = agg;
	}

	return (0);
}

int
dt_aggid_lookup(dtrace_hdl_t *dtp, dtrace_aggid_t aggid, dtrace_aggdesc_t **adp)
{
	int rval;

	if (aggid >= dtp->dt_maxagg || dtp->dt_aggdesc[aggid] == NULL) {
		if ((rval = dt_aggid_add(dtp, aggid)) != 0)
			return (rval);
	}

	assert(aggid < dtp->dt_maxagg);
	assert(dtp->dt_aggdesc[aggid] != NULL);
	*adp = dtp->dt_aggdesc[aggid];

	return (0);
}

void
dt_aggid_destroy(dtrace_hdl_t *dtp)
{
	size_t i;

	assert((dtp->dt_aggdesc != NULL && dtp->dt_maxagg != 0) ||
	    (dtp->dt_aggdesc == NULL && dtp->dt_maxagg == 0));

	if (dtp->dt_aggdesc == NULL)
		return;

	for (i = 0; i < dtp->dt_maxagg; i++) {
		if (dtp->dt_aggdesc[i] != NULL)
			free(dtp->dt_aggdesc[i]);
	}

	free(dtp->dt_aggdesc);
	dtp->dt_aggdesc = NULL;
	dtp->dt_maxagg = 0;
}
