/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <alloca.h>

#include <dt_impl.h>
#include <dt_probe.h>
#include <dt_program.h>

static const char _dt_errprog[] =
"dtrace:::ERROR"
"{"
"	trace(arg1);"
"	trace(arg2);"
"	trace(arg3);"
"	trace(arg4);"
"	trace(arg5);"
"}";

int
dtrace_handle_err(dtrace_hdl_t *dtp, dtrace_handle_err_f *hdlr, void *arg)
{
	dtrace_prog_t *pgp = NULL;
#ifdef FIXME
	dt_stmt_t *stp;
	dtrace_ecbdesc_t *edp;
#endif

	/*
	 * We don't currently support multiple error handlers.
	 */
	if (dtp->dt_errhdlr != NULL)
		return dt_set_errno(dtp, EALREADY);

	/*
	 * If the DTRACEOPT_GRABANON is enabled, the anonymous enabling will
	 * already have a dtrace:::ERROR probe enabled; save 'hdlr' and 'arg'
	 * but do not bother compiling and enabling _dt_errprog.
	 */
	if (dtp->dt_options[DTRACEOPT_GRABANON] != DTRACEOPT_UNSET)
		goto out;

	pgp = dtrace_program_strcompile(dtp, _dt_errprog, DTRACE_PROBESPEC_NAME,
					DTRACE_C_ZDEFS | DTRACE_C_EPROBE, 0,
					NULL);
	if (pgp == NULL)
		return dt_set_errno(dtp, dtrace_errno(dtp));

	if (dtrace_program_exec(dtp, pgp, NULL) == -1)
		return -1;		/* errno already set */

out:
	dtp->dt_errhdlr = hdlr;
	dtp->dt_errarg = arg;

	return 0;
}

int
dtrace_handle_drop(dtrace_hdl_t *dtp, dtrace_handle_drop_f *hdlr, void *arg)
{
	if (dtp->dt_drophdlr != NULL)
		return dt_set_errno(dtp, EALREADY);

	dtp->dt_drophdlr = hdlr;
	dtp->dt_droparg = arg;

	return 0;
}

int
dtrace_handle_proc(dtrace_hdl_t *dtp, dtrace_handle_proc_f *hdlr, void *arg)
{
	if (dtp->dt_prochdlr != NULL)
		return dt_set_errno(dtp, EALREADY);

	dtp->dt_prochdlr = hdlr;
	dtp->dt_procarg = arg;

	return 0;
}

int
dtrace_handle_buffered(dtrace_hdl_t *dtp, dtrace_handle_buffered_f *hdlr,
    void *arg)
{
	if (dtp->dt_bufhdlr != NULL)
		return dt_set_errno(dtp, EALREADY);

	if (hdlr == NULL)
		return dt_set_errno(dtp, EINVAL);

	dtp->dt_bufhdlr = hdlr;
	dtp->dt_bufarg = arg;

	return 0;
}

int
dtrace_handle_setopt(dtrace_hdl_t *dtp, dtrace_handle_setopt_f *hdlr,
    void *arg)
{
	if (hdlr == NULL)
		return dt_set_errno(dtp, EINVAL);

	dtp->dt_setopthdlr = hdlr;
	dtp->dt_setoptarg = arg;

	return 0;
}

#define	DT_REC(type, ndx) *((type *)((uintptr_t)data->dtpda_data + \
    dd->dtdd_recs[(ndx)].dtrd_offset))

static int
dt_handle_err(dtrace_hdl_t *dtp, dtrace_probedata_t *data)
{
	dtrace_datadesc_t *dd = data->dtpda_ddesc, *errdd;
	dtrace_probedesc_t *pd = data->dtpda_pdesc, *errpd;
	dtrace_stmtdesc_t *stp;
	dtrace_errdata_t err;
	dtrace_id_t prid;
	dtrace_stid_t stid;

	char *str, *details, *offinfo;
	int rc = 0;

	assert(dd->dtdd_uarg == DT_ECB_ERROR);

	if (dd->dtdd_nrecs != 5 || strcmp(pd->prv, "dtrace") != 0 ||
	    strcmp(pd->prb, "ERROR") != 0)
		return dt_set_errno(dtp, EDT_BADERROR);

	/*
	 * This is an error.  We have the following items here:  PRID,
	 * statement ID, BPF pc, fault code and faulting address.
	 */
	prid = DT_REC(uint64_t, 0);
	stid = DT_REC(uint64_t, 1);

	if (prid > dtp->dt_probe_id)
		return dt_set_errno(dtp, EDT_BADERROR);
	if (dt_stid_lookup(dtp, stid, &errdd) != 0)
		return dt_set_errno(dtp, EDT_BADERROR);
	errpd = (dtrace_probedesc_t *)dtp->dt_probes[prid]->desc;
	err.dteda_ddesc = errdd;
	err.dteda_pdesc = errpd;
	err.dteda_cpu = data->dtpda_cpu;
	err.dteda_action = stid;
	err.dteda_offset = (int)DT_REC(uint64_t, 2);
	err.dteda_fault = (int)DT_REC(uint64_t, 3);
	err.dteda_addr = DT_REC(uint64_t, 4);

	if (err.dteda_offset != -1)
		asprintf(&offinfo, " at BPF pc %d", err.dteda_offset);
	else
		offinfo = "";

	switch (err.dteda_fault) {
	case DTRACEFLT_BADADDR:
		if ((int64_t)err.dteda_addr == -1)
			goto no_addr;
	case DTRACEFLT_BADALIGN:
	case DTRACEFLT_BADSTACK:
	case DTRACEFLT_BADSIZE:
		asprintf(&details, " (0x%llx)", (unsigned long long)err.dteda_addr);
		break;
	case DTRACEFLT_BADINDEX:
		asprintf(&details, " (%ld)", (int64_t)err.dteda_addr);
		break;

	default:
no_addr:
		details = "";
	}

	stp = dtp->dt_stmts[stid];
	assert(stp != NULL);
	asprintf(&str, "error in %s for probe ID %u (%s:%s:%s:%s): %s%s%s",
		 stp->dtsd_clause->di_name, errpd->id, errpd->prv, errpd->mod,
		 errpd->fun, errpd->prb, dtrace_faultstr(dtp, err.dteda_fault),
		 details, offinfo);

	err.dteda_msg = str;

	if (dtp->dt_errhdlr == NULL)
		rc = dt_set_errno(dtp, EDT_ERRABORT);
	else if ((*dtp->dt_errhdlr)(&err, dtp->dt_errarg) == DTRACE_HANDLE_ABORT)
		rc = dt_set_errno(dtp, EDT_ERRABORT);

	free(str);
	if (offinfo[0] != 0)
		free(offinfo);
	if (details[0] != 0)
		free(details);

	return rc;
}

int
dt_handle_rawerr(dtrace_hdl_t *dtp, const char *errmsg)
{
	dtrace_errdata_t	err;

	err.dteda_ddesc = NULL;
	err.dteda_pdesc = NULL;
	err.dteda_cpu = -1;
	err.dteda_action = -1;
	err.dteda_offset = -1;
	err.dteda_fault = DTRACEFLT_LIBRARY;
	err.dteda_addr = 0;
	err.dteda_msg = errmsg;

	if ((*dtp->dt_errhdlr)(&err, dtp->dt_errarg) == DTRACE_HANDLE_ABORT)
		return dt_set_errno(dtp, EDT_ERRABORT);

	return 0;
}

int
dt_handle_liberr(dtrace_hdl_t *dtp, const dtrace_probedata_t *data,
    const char *faultstr)
{
	dtrace_probedesc_t *errpd = data->dtpda_pdesc;
	dtrace_stmtdesc_t *stp;
	dtrace_errdata_t err;
	char *str;
	int rc = 0;

	err.dteda_ddesc = data->dtpda_ddesc;
	err.dteda_pdesc = errpd;
	err.dteda_cpu = data->dtpda_cpu;
	err.dteda_action = -1;
	err.dteda_offset = -1;
	err.dteda_fault = DTRACEFLT_LIBRARY;
	err.dteda_addr = 0; /* == NULL */

	stp = dtp->dt_stmts[data->dtpda_stid];
	assert(stp != NULL);
	asprintf(&str,
		 "error in %s for probe ID %u (%s:%s:%s:%s): %s",
		 stp->dtsd_clause->di_name, errpd->id, errpd->prv, errpd->mod,
		 errpd->fun, errpd->prb, faultstr);

	err.dteda_msg = str;

	if (dtp->dt_errhdlr == NULL)
		rc = dt_set_errno(dtp, EDT_ERRABORT);
	else if ((*dtp->dt_errhdlr)(&err, dtp->dt_errarg) == DTRACE_HANDLE_ABORT)
		rc = dt_set_errno(dtp, EDT_ERRABORT);

	free(str);

	return rc;
}

#define	DROPTAG(x)	x, #x

static const struct {
	dtrace_dropkind_t dtdrg_kind;
	char *dtdrg_tag;
} _dt_droptags[] = {
	{ DROPTAG(DTRACEDROP_PRINCIPAL) },
	{ DROPTAG(DTRACEDROP_AGGREGATION) },
	{ DROPTAG(DTRACEDROP_DYNAMIC) },
	{ DROPTAG(DTRACEDROP_DYNRINSE) },
	{ DROPTAG(DTRACEDROP_DYNDIRTY) },
	{ DROPTAG(DTRACEDROP_SPEC) },
	{ DROPTAG(DTRACEDROP_SPECBUSY) },
	{ DROPTAG(DTRACEDROP_SPECUNAVAIL) },
	{ DROPTAG(DTRACEDROP_DBLERROR) },
	{ DROPTAG(DTRACEDROP_STKSTROVERFLOW) },
	{ 0, NULL }
};

static const char *
dt_droptag(dtrace_dropkind_t kind)
{
	int i;

	for (i = 0; _dt_droptags[i].dtdrg_tag != NULL; i++) {
		if (_dt_droptags[i].dtdrg_kind == kind)
			return _dt_droptags[i].dtdrg_tag;
	}

	return "DTRACEDROP_UNKNOWN";
}

int
dt_handle_cpudrop(dtrace_hdl_t *dtp, processorid_t cpu,
    dtrace_dropkind_t what, uint64_t howmany)
{
	dtrace_dropdata_t drop;
	char str[80], *s;
	int size;

	assert(what == DTRACEDROP_PRINCIPAL || what == DTRACEDROP_AGGREGATION);

	if (howmany == 0)
		return 0;

	memset(&drop, 0, sizeof(drop));
	drop.dtdda_handle = dtp;
	drop.dtdda_cpu = cpu;
	drop.dtdda_kind = what;
	drop.dtdda_drops = howmany;
	drop.dtdda_msg = str;

	if (dtp->dt_droptags) {
		snprintf(str, sizeof(str), "[%s] ", dt_droptag(what));
		s = &str[strlen(str)];
		size = sizeof(str) - (s - str);
	} else {
		s = str;
		size = sizeof(str);
	}

	snprintf(s, size, "%llu %sdrop%s on CPU %d",
		 (unsigned long long)howmany,
		 what == DTRACEDROP_PRINCIPAL ? "" : "aggregation ",
		 howmany > 1 ? "s" : "", cpu);

	if (dtp->dt_drophdlr == NULL)
		return dt_set_errno(dtp, EDT_DROPABORT);

	if ((*dtp->dt_drophdlr)(&drop, dtp->dt_droparg) == DTRACE_HANDLE_ABORT)
		return dt_set_errno(dtp, EDT_DROPABORT);

	return 0;
}

static const struct {
	dtrace_dropkind_t dtdrt_kind;
	uintptr_t dtdrt_offset;
	const char *dtdrt_str;
	const char *dtdrt_msg;
} _dt_droptab[] = {
	{ DTRACEDROP_DYNAMIC,
	    offsetof(dtrace_status_t, dtst_dyndrops),
	    "dynamic variable drop" },

	{ DTRACEDROP_DYNRINSE,
	    offsetof(dtrace_status_t, dtst_dyndrops_rinsing),
	    "dynamic variable drop", " with non-empty rinsing list" },

	{ DTRACEDROP_DYNDIRTY,
	    offsetof(dtrace_status_t, dtst_dyndrops_dirty),
	    "dynamic variable drop", " with non-empty dirty list" },

	{ DTRACEDROP_SPEC,
	    offsetof(dtrace_status_t, dtst_specdrops),
	    "speculative drop" },

	{ DTRACEDROP_SPECBUSY,
	    offsetof(dtrace_status_t, dtst_specdrops_busy),
	    "failed speculation", " (available buffer(s) still busy)" },

	{ DTRACEDROP_SPECUNAVAIL,
	    offsetof(dtrace_status_t, dtst_specdrops_unavail),
	    "failed speculation", " (no speculative buffer available)" },

	{ DTRACEDROP_STKSTROVERFLOW,
	    offsetof(dtrace_status_t, dtst_stkstroverflows),
	    "jstack()/ustack() string table overflow" },

	{ DTRACEDROP_DBLERROR,
	    offsetof(dtrace_status_t, dtst_dblerrors),
	    "error", " in ERROR probe enabling" },

	{ 0, 0, NULL }
};

int
dt_handle_status(dtrace_hdl_t *dtp, dtrace_status_t *old, dtrace_status_t *new)
{
	dtrace_dropdata_t drop;
	char str[80], *s;
	uintptr_t base = (uintptr_t)new, obase = (uintptr_t)old;
	int i, size;

	memset(&drop, 0, sizeof(drop));
	drop.dtdda_handle = dtp;
	drop.dtdda_cpu = DTRACE_CPUALL;
	drop.dtdda_msg = str;

	/*
	 * First, check to see if we've been killed -- in which case we abort.
	 */
	if (new->dtst_killed && !old->dtst_killed)
		return dt_set_errno(dtp, EDT_BRICKED);

	for (i = 0; _dt_droptab[i].dtdrt_str != NULL; i++) {
		uintptr_t naddr = base + _dt_droptab[i].dtdrt_offset;
		uintptr_t oaddr = obase + _dt_droptab[i].dtdrt_offset;

		uint64_t nval = *((uint64_t *)naddr);
		uint64_t oval = *((uint64_t *)oaddr);

		if (nval == oval)
			continue;

		if (dtp->dt_droptags) {
			snprintf(str, sizeof(str), "[%s] ",
			    dt_droptag(_dt_droptab[i].dtdrt_kind));
			s = &str[strlen(str)];
			size = sizeof(str) - (s - str);
		} else {
			s = str;
			size = sizeof(str);
		}

		snprintf(s, size, "%llu %s%s%s",
		    (unsigned long long)nval - oval,
		    _dt_droptab[i].dtdrt_str, (nval - oval > 1) ? "s" : "",
		    _dt_droptab[i].dtdrt_msg != NULL ?
		    _dt_droptab[i].dtdrt_msg : "");

		drop.dtdda_kind = _dt_droptab[i].dtdrt_kind;
		drop.dtdda_total = nval;
		drop.dtdda_drops = nval - oval;

		if (dtp->dt_drophdlr == NULL)
			return dt_set_errno(dtp, EDT_DROPABORT);

		if ((*dtp->dt_drophdlr)(&drop,
		    dtp->dt_droparg) == DTRACE_HANDLE_ABORT)
			return dt_set_errno(dtp, EDT_DROPABORT);
	}

	return 0;
}

int
dt_handle_setopt(dtrace_hdl_t *dtp, dtrace_setoptdata_t *data)
{
	void *arg = dtp->dt_setoptarg;

	if (dtp->dt_setopthdlr == NULL)
		return 0;

	if ((*dtp->dt_setopthdlr)(data, arg) == DTRACE_HANDLE_ABORT)
		return dt_set_errno(dtp, EDT_DIRABORT);

	return 0;
}

int
dt_handle(dtrace_hdl_t *dtp, dtrace_probedata_t *data)
{
	dtrace_datadesc_t *dd = data->dtpda_ddesc;
	int rval;

	switch (dd->dtdd_uarg) {
	case DT_ECB_ERROR:
		rval = dt_handle_err(dtp, data);
		break;

	default:
		return DTRACE_CONSUME_THIS;
	}

	if (rval == 0)
		return DTRACE_CONSUME_NEXT;

	return DTRACE_CONSUME_ERROR;
}
