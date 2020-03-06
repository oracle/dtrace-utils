/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <dt_impl.h>
#include <dt_probe.h>
#include <dt_printf.h>

#if 0
static int
dt_epid_add(dtrace_hdl_t *dtp, dtrace_epid_t id)
{
	dtrace_id_t max;
	int rval, i, maxformat;
	dtrace_eprobedesc_t *enabled, *nenabled;
	dtrace_probedesc_t *probe;

	while (id >= (max = dtp->dt_maxprobe) || dtp->dt_pdesc == NULL) {
		dtrace_id_t new_max = max ? (max << 1) : 1;
		size_t nsize = new_max * sizeof (void *);
		dtrace_probedesc_t **new_pdesc;
		dtrace_eprobedesc_t **new_edesc;

		if ((new_pdesc = malloc(nsize)) == NULL ||
		    (new_edesc = malloc(nsize)) == NULL) {
			free(new_pdesc);
			return (dt_set_errno(dtp, EDT_NOMEM));
		}

		memset(new_pdesc, 0, nsize);
		memset(new_edesc, 0, nsize);

		if (dtp->dt_pdesc != NULL) {
			size_t osize = max * sizeof (void *);

			memcpy(new_pdesc, dtp->dt_pdesc, osize);
			free(dtp->dt_pdesc);

			memcpy(new_edesc, dtp->dt_edesc, osize);
			free(dtp->dt_edesc);
		}

		dtp->dt_pdesc = new_pdesc;
		dtp->dt_edesc = new_edesc;
		dtp->dt_maxprobe = new_max;
	}

	if (dtp->dt_pdesc[id] != NULL)
		return (0);

	if ((enabled = malloc(sizeof (dtrace_eprobedesc_t))) == NULL)
		return (dt_set_errno(dtp, EDT_NOMEM));

	memset(enabled, 0, sizeof (dtrace_eprobedesc_t));
	enabled->dtepd_epid = id;
	enabled->dtepd_nrecs = 1;

	if (dt_ioctl(dtp, DTRACEIOC_EPROBE, enabled) == -1) {
		rval = dt_set_errno(dtp, errno);
		free(enabled);
		return (rval);
	}

	if (DTRACE_SIZEOF_EPROBEDESC(enabled) != sizeof (*enabled)) {
		/*
		 * There must be more than one action.  Allocate the
		 * appropriate amount of space and try again.
		 */
		if ((nenabled =
		    malloc(DTRACE_SIZEOF_EPROBEDESC(enabled))) != NULL)
			memcpy(nenabled, enabled, sizeof (*enabled));

		free(enabled);

		if ((enabled = nenabled) == NULL)
			return (dt_set_errno(dtp, EDT_NOMEM));

		rval = dt_ioctl(dtp, DTRACEIOC_EPROBE, enabled);

		if (rval == -1) {
			rval = dt_set_errno(dtp, errno);
			free(enabled);
			return (rval);
		}
	}

	if ((probe = malloc(sizeof (dtrace_probedesc_t))) == NULL) {
		free(enabled);
		return (dt_set_errno(dtp, EDT_NOMEM));
	}

	probe->id = enabled->dtepd_probeid;

	if (dt_ioctl(dtp, DTRACEIOC_PROBES, probe) == -1) {
		rval = dt_set_errno(dtp, errno);
		goto err;
	}

	for (i = 0; i < enabled->dtepd_nrecs; i++) {
		dtrace_fmtdesc_t fmt;
		dtrace_recdesc_t *rec = &enabled->dtepd_rec[i];

		if (!DTRACEACT_ISPRINTFLIKE(rec->dtrd_action))
			continue;

		if (rec->dtrd_format == 0)
			continue;

		if (rec->dtrd_format <= dtp->dt_maxformat &&
		    dtp->dt_formats[rec->dtrd_format - 1] != NULL)
			continue;

		memset(&fmt, 0, sizeof (fmt));
		fmt.dtfd_format = rec->dtrd_format;
		fmt.dtfd_string = NULL;
		fmt.dtfd_length = 0;

		if (dt_ioctl(dtp, DTRACEIOC_FORMAT, &fmt) == -1) {
			rval = dt_set_errno(dtp, errno);
			goto err;
		}

		if ((fmt.dtfd_string = malloc(fmt.dtfd_length)) == NULL) {
			rval = dt_set_errno(dtp, EDT_NOMEM);
			goto err;
		}

		if (dt_ioctl(dtp, DTRACEIOC_FORMAT, &fmt) == -1) {
			rval = dt_set_errno(dtp, errno);
			free(fmt.dtfd_string);
			goto err;
		}

		while (rec->dtrd_format > (maxformat = dtp->dt_maxformat)) {
			int new_max = maxformat ? (maxformat << 1) : 1;
			size_t nsize = new_max * sizeof (void *);
			size_t osize = maxformat * sizeof (void *);
			void **new_formats = malloc(nsize);

			if (new_formats == NULL) {
				rval = dt_set_errno(dtp, EDT_NOMEM);
				free(fmt.dtfd_string);
				goto err;
			}

			memset(new_formats, 0, nsize);
			memcpy(new_formats, dtp->dt_formats, osize);
			free(dtp->dt_formats);

			dtp->dt_formats = new_formats;
			dtp->dt_maxformat = new_max;
		}

		dtp->dt_formats[rec->dtrd_format - 1] =
		    rec->dtrd_action == DTRACEACT_PRINTA ?
		    dtrace_printa_create(dtp, fmt.dtfd_string) :
		    dtrace_printf_create(dtp, fmt.dtfd_string);

		free(fmt.dtfd_string);

		if (dtp->dt_formats[rec->dtrd_format - 1] == NULL) {
			rval = -1; /* dt_errno is set for us */
			goto err;
		}
	}

	dtp->dt_pdesc[id] = probe;
	dtp->dt_edesc[id] = enabled;

	return (0);

err:
	/*
	 * If we failed, free our allocated probes.  Note that if we failed
	 * while allocating formats, we aren't going to free formats that
	 * we have already allocated.  This is okay; these formats are
	 * hanging off of dt_formats and will therefore not be leaked.
	 */
	free(enabled);
	free(probe);
	return (rval);
}
#else
dtrace_epid_t
dt_epid_add(dtrace_hdl_t *dtp, dtrace_id_t prid, int nrecs)
{
	dtrace_id_t		max;
	dtrace_epid_t		epid;
	dtrace_eprobedesc_t	*eprobe;

	epid = dtp->dt_nextepid++;
	max = dtp->dt_maxprobe;
	if (epid >= max || dtp->dt_edesc == NULL) {
		dtrace_id_t		nmax = max ? (max << 1) : 2;
		dtrace_eprobedesc_t	**nedesc;
		dtrace_probedesc_t	**npdesc;

		nedesc = dt_calloc(dtp, nmax, sizeof(void *));
		npdesc = dt_calloc(dtp, nmax, sizeof(void *));
		if (nedesc == NULL || npdesc == NULL) {
			dt_free(dtp, nedesc);
			dt_free(dtp, npdesc);
			return dt_set_errno(dtp, EDT_NOMEM);
		}

		if (dtp->dt_edesc != NULL) {
			size_t	osize = max * sizeof(void *);

			memcpy(nedesc, dtp->dt_edesc, osize);
			dt_free(dtp, dtp->dt_edesc);
			memcpy(npdesc, dtp->dt_pdesc, osize);
			dt_free(dtp, dtp->dt_pdesc);
		}

		dtp->dt_edesc = nedesc;
		dtp->dt_pdesc = npdesc;
		dtp->dt_maxprobe = nmax;
	}

	if (dtp->dt_edesc[epid] != NULL)
		return epid;

	/*
	 * This is a bit odd, but we want to make sure that nrecs is not a
	 * negative value while also taking into consideration that there is
	 * always rooms for one record in dtrace_eprobedesc_t, so for our
	 * allocation purposes we account for that default record slot.
	 */
	assert(nrecs >= 0);
	if (nrecs == 0)
		nrecs = 1;

	eprobe = dt_zalloc(dtp, sizeof(dtrace_eprobedesc_t) +
				(nrecs - 1) * sizeof(dtrace_recdesc_t));
	if (eprobe == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	eprobe->dtepd_epid = epid;
	eprobe->dtepd_probeid = prid;
	eprobe->dtepd_nrecs = nrecs;

	dtp->dt_edesc[epid] = eprobe;
	dtp->dt_pdesc[epid] = (dtrace_probedesc_t *)dtp->dt_probes[prid]->desc;

	return epid;
}
#endif

int
dt_epid_lookup(dtrace_hdl_t *dtp, dtrace_epid_t epid,
    dtrace_eprobedesc_t **epdp, dtrace_probedesc_t **pdp)
{
	int rval;

#if 0
	if (epid >= dtp->dt_maxprobe || dtp->dt_pdesc[epid] == NULL) {
		if ((rval = dt_epid_add(dtp, epid)) != 0)
			return (rval);
	}
#endif

	assert(epid < dtp->dt_maxprobe);
	assert(dtp->dt_edesc[epid] != NULL);
	assert(dtp->dt_pdesc[epid] != NULL);
	*epdp = dtp->dt_edesc[epid];
	*pdp = dtp->dt_pdesc[epid];

	return (0);
}

void
dt_epid_destroy(dtrace_hdl_t *dtp)
{
	size_t i;

	assert((dtp->dt_pdesc != NULL && dtp->dt_edesc != NULL &&
	    dtp->dt_maxprobe > 0) || (dtp->dt_pdesc == NULL &&
	    dtp->dt_edesc == NULL && dtp->dt_maxprobe == 0));

	if (dtp->dt_pdesc == NULL)
		return;

	for (i = 0; i < dtp->dt_maxprobe; i++) {
		if (dtp->dt_edesc[i] == NULL) {
			assert(dtp->dt_pdesc[i] == NULL);
			continue;
		}

		assert(dtp->dt_pdesc[i] != NULL);
		free(dtp->dt_edesc[i]);
		free(dtp->dt_pdesc[i]);
	}

	free(dtp->dt_pdesc);
	dtp->dt_pdesc = NULL;

	free(dtp->dt_edesc);
	dtp->dt_edesc = NULL;
	dtp->dt_nextepid = 0;
	dtp->dt_maxprobe = 0;
}

void *
dt_format_lookup(dtrace_hdl_t *dtp, int format)
{
	if (format == 0 || format > dtp->dt_maxformat)
		return (NULL);

	if (dtp->dt_formats == NULL)
		return (NULL);

	return (dtp->dt_formats[format - 1]);
}

void
dt_format_destroy(dtrace_hdl_t *dtp)
{
	int i;

	for (i = 0; i < dtp->dt_maxformat; i++) {
		if (dtp->dt_formats[i] != NULL)
			dt_printf_destroy(dtp->dt_formats[i]);
	}

	free(dtp->dt_formats);
	dtp->dt_formats = NULL;
}

static int
dt_aggid_add(dtrace_hdl_t *dtp, dtrace_aggid_t id)
{
	dtrace_id_t max;
	dtrace_epid_t epid;
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
dt_aggid_lookup(dtrace_hdl_t *dtp, dtrace_aggid_t aggid,
    dtrace_aggdesc_t **adp)
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
