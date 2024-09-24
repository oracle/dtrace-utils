/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <alloca.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_printf.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_bpf.h>

dtrace_prog_t *
dt_program_create(dtrace_hdl_t *dtp)
{
	dtrace_prog_t *pgp = dt_zalloc(dtp, sizeof(dtrace_prog_t));

	if (pgp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	dt_list_append(&dtp->dt_programs, pgp);

	/*
	 * By default, programs start with DOF version 1 so that output files
	 * containing DOF are backward compatible. If a program requires new
	 * DOF features, the version is increased as needed.
	 */
	pgp->dp_dofversion = DOF_VERSION_1;

	return pgp;
}

void
dt_program_destroy(dtrace_hdl_t *dtp, dtrace_prog_t *pgp)
{
	dt_stmt_t *stp, *next;
	uint_t i;

	for (stp = dt_list_next(&pgp->dp_stmts); stp != NULL; stp = next) {
		next = dt_list_next(stp);
		dtrace_stmt_destroy(dtp, stp->ds_desc);
		dt_free(dtp, stp);
	}

	for (i = 0; i < pgp->dp_xrefslen; i++)
		dt_free(dtp, pgp->dp_xrefs[i]);

	dt_free(dtp, pgp->dp_xrefs);
	dt_list_delete(&dtp->dt_programs, pgp);
	dt_free(dtp, pgp);
}

/*ARGSUSED*/
void
dtrace_program_info(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_proginfo_t *pip)
{
	dt_stmt_t *stp;
	dtrace_ecbdesc_t *last = NULL;

	if (pip == NULL)
		return;

	memset(pip, 0, sizeof(dtrace_proginfo_t));

	if (dt_list_next(&pgp->dp_stmts) != NULL) {
		pip->dpi_descattr = _dtrace_maxattr;
		pip->dpi_stmtattr = _dtrace_maxattr;
	} else {
		pip->dpi_descattr = _dtrace_defattr;
		pip->dpi_stmtattr = _dtrace_defattr;
	}

	for (stp = dt_list_next(&pgp->dp_stmts); stp; stp = dt_list_next(stp)) {
		dtrace_ecbdesc_t	*edp = stp->ds_desc->dtsd_ecbdesc;
		dtrace_difo_t		*dp;
		dtrace_datadesc_t	*ddp = NULL;

		if (edp == last)
			continue;
		last = edp;

		pip->dpi_descattr =
		    dt_attr_min(stp->ds_desc->dtsd_descattr, pip->dpi_descattr);

		pip->dpi_stmtattr =
		    dt_attr_min(stp->ds_desc->dtsd_stmtattr, pip->dpi_stmtattr);

		/*
		 * If there aren't any actions, account for the fact that
		 * the default action will generate a record.
		 */
		dp = dt_dlib_get_func_difo(dtp, stp->ds_desc->dtsd_clause);
		if (dp != NULL)
			ddp = dp->dtdo_ddesc;
		if (ddp == NULL || ddp->dtdd_nrecs == 0)
			pip->dpi_recgens++;
		else
			pip->dpi_recgens += ddp->dtdd_nrecs;

#if 0
		for (ap = edp->dted_action; ap != NULL; ap = ap->dtad_next) {
			if (ap->dtad_kind == DTRACEACT_SPECULATE) {
				pip->dpi_speculations++;
				continue;
			}

			if (DTRACEACT_ISAGG(ap->dtad_kind)) {
				pip->dpi_recgens -= ap->dtad_arg;
				pip->dpi_aggregates++;
				continue;
			}

			if (DTRACEACT_ISDESTRUCTIVE(ap->dtad_kind))
				continue;

			if (ap->dtad_kind == DTRACEACT_DIFEXPR &&
			    ap->dtad_difo->dtdo_rtype.dtdt_kind ==
			    DIF_TYPE_CTF &&
			    ap->dtad_difo->dtdo_rtype.dtdt_size == 0)
				continue;

			pip->dpi_recgens++;
		}
#endif
	}
}

typedef struct pi_state {
	int			*cnt;
	dtrace_stmtdesc_t	*sdp;
} pi_state_t;

static int
dt_stmt_probe(dtrace_hdl_t *dtp, dt_probe_t *prp, pi_state_t *st)
{
	dtrace_probeinfo_t	p;

	dt_probe_info(dtp, prp->desc, &p);
	dt_probe_enable(dtp, prp);

	dt_probe_add_stmt(dtp, prp, st->sdp);
	(*st->cnt)++;

	return 0;
}

static int
dt_prog_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *sdp,
	     int *cnt)
{
	pi_state_t		st;
	dtrace_probedesc_t	*pdp = &sdp->dtsd_ecbdesc->dted_probe;
	int			rc;

	if (dtp->dt_stmts == NULL) {
		dtp->dt_stmts = dt_calloc(dtp, dtp->dt_stmt_nextid, sizeof(dtrace_stmtdesc_t *));
		if (dtp->dt_stmts == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);
	}
	dtp->dt_stmts[sdp->dtsd_id] = sdp;

	st.cnt = cnt;
	st.sdp = sdp;
	rc = dt_probe_iter(dtp, pdp, (dt_probe_f *)dt_stmt_probe, NULL, &st);

	/*
	 * At this point, dtp->dt_cflags has no information about
	 * DTRACE_C_ZDEFS.  Do not worry about it.  If the probe
	 * definition matches no probes and DTRACE_C_ZDEFS had not
	 * been set, the problem would have been reported earlier,
	 * in dt_setcontext().
	 */
	if (rc && dtrace_errno(dtp) == EDT_NOPROBE)
		return 0;
	return rc;
}

int
dtrace_program_exec(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
		    dtrace_proginfo_t *pip)
{
	int	cnt = 0;
	int	rc = 0;

	dtrace_program_info(dtp, pgp, pip);
	rc = dtrace_stmt_iter(dtp, pgp, (dtrace_stmt_f *)dt_prog_stmt, &cnt);
	if (rc < 0)
		return rc;

	if (pip != NULL)
		pip->dpi_matches += cnt;

	return 0;
}

static void
dt_ecbdesc_hold(dtrace_ecbdesc_t *edp)
{
	edp->dted_refcnt++;
}

void
dt_ecbdesc_release(dtrace_hdl_t *dtp, dtrace_ecbdesc_t *edp)
{
	if (--edp->dted_refcnt > 0)
		return;

	dt_free(dtp, edp);
}

dtrace_ecbdesc_t *
dt_ecbdesc_create(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	dtrace_ecbdesc_t *edp;

	if ((edp = dt_zalloc(dtp, sizeof(dtrace_ecbdesc_t))) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	edp->dted_probe = *pdp;
	dt_ecbdesc_hold(edp);
	return edp;
}

dtrace_stmtdesc_t *
dtrace_stmt_create(dtrace_hdl_t *dtp, dtrace_ecbdesc_t *edp)
{
	dtrace_stmtdesc_t	*sdp;

	sdp = dt_zalloc(dtp, sizeof(dtrace_stmtdesc_t));
	if (sdp == NULL)
		return NULL;

	dt_ecbdesc_hold(edp);
	sdp->dtsd_ecbdesc = edp;
	sdp->dtsd_descattr = _dtrace_defattr;
	sdp->dtsd_stmtattr = _dtrace_defattr;

	return sdp;
}

int
dtrace_stmt_add(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *sdp)
{
	dt_stmt_t *stp = dt_alloc(dtp, sizeof(dt_stmt_t));

	if (stp == NULL)
		return -1; /* errno is set for us */

	dt_list_append(&pgp->dp_stmts, stp);
	stp->ds_desc = sdp;

	return 0;
}

int
dtrace_stmt_iter(dtrace_hdl_t *dtp, dtrace_prog_t *pgp,
    dtrace_stmt_f *func, void *data)
{
	dt_stmt_t *stp, *next;
	int status = 0;

	for (stp = dt_list_next(&pgp->dp_stmts); stp != NULL; stp = next) {
		next = dt_list_next(stp);
		if ((status = func(dtp, pgp, stp->ds_desc, data)) != 0)
			break;
	}

	return status;
}

void
dtrace_stmt_destroy(dtrace_hdl_t *dtp, dtrace_stmtdesc_t *sdp)
{
	if (sdp->dtsd_fmtdata != NULL)
		dt_printf_destroy(sdp->dtsd_fmtdata);

	dt_ecbdesc_release(dtp, sdp->dtsd_ecbdesc);
	dt_free(dtp, sdp);
}

typedef struct dt_header_info {
	dtrace_hdl_t *dthi_dtp;	/* consumer handle */
	FILE *dthi_out;		/* output file */
	char *dthi_pmname;	/* provider macro name */
	char *dthi_pfname;	/* provider function name */
	int dthi_empty;		/* should we generate empty macros */
} dt_header_info_t;

static void
dt_header_fmt_macro(char *buf, const char *str)
{
	for (;;) {
		if (islower(*str)) {
			*buf++ = *str++ + 'A' - 'a';
		} else if (*str == '-') {
			*buf++ = '_';
			str++;
		} else if (*str == '.') {
			*buf++ = '_';
			str++;
		} else if ((*buf++ = *str++) == '\0') {
			break;
		}
	}
}

static void
dt_header_fmt_func(char *buf, const char *str)
{
	for (;;) {
		if (*str == '-') {
			*buf++ = '_';
			*buf++ = '_';
			str++;
		} else if ((*buf++ = *str++) == '\0') {
			break;
		}
	}
}

/*ARGSUSED*/
static int
dt_header_decl(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	dt_header_info_t *infop = data;
	dtrace_hdl_t *dtp = infop->dthi_dtp;
	dt_probe_t *prp = idp->di_data;
	dt_node_t *dnp;
	char buf[DT_TYPE_NAMELEN];
	char *fname;
	const char *p;
	int i;

	p = prp->pr_name;
	for (i = 0; (p = strchr(p, '-')) != NULL; i++)
		p++;

	fname = alloca(strlen(prp->pr_name) + 1 + i);
	dt_header_fmt_func(fname, prp->pr_name);

	if (fprintf(infop->dthi_out, "extern void __dtrace_%s___%s(",
	    infop->dthi_pfname, fname) < 0)
		return dt_set_errno(dtp, errno);

	for (dnp = prp->nargs, i = 0; dnp != NULL; dnp = dnp->dn_list, i++) {
		if (fprintf(infop->dthi_out, "%s",
		    ctf_type_name(dnp->dn_ctfp, dnp->dn_type,
		    buf, sizeof(buf))) < 0)
			return dt_set_errno(dtp, errno);

		if (i + 1 < prp->nargc &&
		    fprintf(infop->dthi_out, ", ") < 0)
			return dt_set_errno(dtp, errno);
	}

	if (i == 0 && fprintf(infop->dthi_out, "void") < 0)
		return dt_set_errno(dtp, errno);

	if (fprintf(infop->dthi_out, ");\n") < 0)
		return dt_set_errno(dtp, errno);

	if (fprintf(infop->dthi_out,
	    "extern void __dtraceenabled_%s___%s(uint32_t *flag);\n",
	    infop->dthi_pfname, fname) < 0)
		return dt_set_errno(dtp, errno);

	return 0;
}

/*ARGSUSED*/
static int
dt_header_probe(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	dt_header_info_t *infop = data;
	dtrace_hdl_t *dtp = infop->dthi_dtp;
	dt_probe_t *prp = idp->di_data;
	char *mname, *fname;
	const char *p;
	int i;

	p = prp->pr_name;
	for (i = 0; (p = strchr(p, '-')) != NULL; i++)
		p++;

	mname = alloca(strlen(prp->pr_name) + 1);
	dt_header_fmt_macro(mname, prp->pr_name);

	fname = alloca(strlen(prp->pr_name) + 1 + i);
	dt_header_fmt_func(fname, prp->pr_name);

	if (fprintf(infop->dthi_out, "#define\t%s_%s(",
	    infop->dthi_pmname, mname) < 0)
		return dt_set_errno(dtp, errno);

	for (i = 0; i < prp->nargc; i++) {
		if (fprintf(infop->dthi_out, "arg%d", i) < 0)
			return dt_set_errno(dtp, errno);

		if (i + 1 != prp->nargc &&
		    fprintf(infop->dthi_out, ", ") < 0)
			return dt_set_errno(dtp, errno);
	}

	if (!infop->dthi_empty) {
		if (fprintf(infop->dthi_out, ") \\\n\t") < 0)
			return dt_set_errno(dtp, errno);

		if (fprintf(infop->dthi_out, "__dtrace_%s___%s(",
		    infop->dthi_pfname, fname) < 0)
			return dt_set_errno(dtp, errno);

		for (i = 0; i < prp->nargc; i++) {
			if (fprintf(infop->dthi_out, "arg%d", i) < 0)
				return dt_set_errno(dtp, errno);

			if (i + 1 != prp->nargc &&
			    fprintf(infop->dthi_out, ", ") < 0)
				return dt_set_errno(dtp, errno);
		}
	}

	if (fprintf(infop->dthi_out, ")\n") < 0)
		return dt_set_errno(dtp, errno);

	if (!infop->dthi_empty) {
		if (fprintf(infop->dthi_out,
		    "#ifdef __GNUC__\n"
		    "#define\t%s_%s_ENABLED() \\\n"
		    "\t({ uint32_t enabled = 0; \\\n"
		    "\t__dtraceenabled_%s___%s(&enabled); \\\n"
		    "\t   enabled; })\n"
		    "#else\n"
		    "#define\t%s_%s_ENABLED() (1)\\n"
		    "#endif\n"
		    "#endif\n",
		    infop->dthi_pmname, mname,
		    infop->dthi_pfname, fname,
		    infop->dthi_pmname, mname) < 0)
			return dt_set_errno(dtp, errno);

	} else {
		if (fprintf(infop->dthi_out, "#define\t%s_%s_ENABLED() (0)\n",
		    infop->dthi_pmname, mname) < 0)
			return dt_set_errno(dtp, errno);
	}

	return 0;
}

static int
dt_header_provider(dtrace_hdl_t *dtp, dt_provider_t *pvp, FILE *out)
{
	dt_header_info_t info;
	const char *p;
	int i;

	if (pvp->pv_flags & DT_PROVIDER_IMPL)
		return 0;

	/*
	 * Count the instances of the '-' character since we'll need to double
	 * those up.
	 */
	p = pvp->desc.dtvd_name;
	for (i = 0; (p = strchr(p, '-')) != NULL; i++)
		p++;

	info.dthi_dtp = dtp;
	info.dthi_out = out;
	info.dthi_empty = 0;

	info.dthi_pmname = alloca(strlen(pvp->desc.dtvd_name) + 1);
	dt_header_fmt_macro(info.dthi_pmname, pvp->desc.dtvd_name);

	info.dthi_pfname = alloca(strlen(pvp->desc.dtvd_name) + 1 + i);
	dt_header_fmt_func(info.dthi_pfname, pvp->desc.dtvd_name);

	if (fprintf(out, "#define _DTRACE_VERSION 1\n\n"
			 "#if _DTRACE_VERSION\n\n") < 0)
		return dt_set_errno(dtp, errno);

	if (dt_idhash_iter(pvp->pv_probes, dt_header_probe, &info) != 0)
		return -1; /* dt_errno is set for us */
	if (fprintf(out, "\n\n") < 0)
		return dt_set_errno(dtp, errno);
	if (dt_idhash_iter(pvp->pv_probes, dt_header_decl, &info) != 0)
		return -1; /* dt_errno is set for us */

	if (fprintf(out, "\n#else\n\n") < 0)
		return dt_set_errno(dtp, errno);

	info.dthi_empty = 1;

	if (dt_idhash_iter(pvp->pv_probes, dt_header_probe, &info) != 0)
		return -1; /* dt_errno is set for us */

	if (fprintf(out, "\n#endif\n\n") < 0)
		return dt_set_errno(dtp, errno);

	return 0;
}

int
dtrace_program_header(dtrace_hdl_t *dtp, FILE *out, const char *fname)
{
	dt_provider_t *pvp;
	dt_htab_next_t *it = NULL;
	char *mfname = NULL, *p;

	if (fname != NULL) {
		if ((p = strrchr(fname, '/')) != NULL)
			fname = p + 1;

		mfname = alloca(strlen(fname) + 1);
		dt_header_fmt_macro(mfname, fname);
		if (fprintf(out, "#ifndef\t_%s\n#define\t_%s\n\n",
		    mfname, mfname) < 0)
			return dt_set_errno(dtp, errno);
	}

	if (fprintf(out, "#include <unistd.h>\n"
		"#include <inttypes.h>\n\n") < 0)
		return -1;

	if (fprintf(out, "#ifdef\t__cplusplus\nextern \"C\" {\n#endif\n\n") < 0)
		return -1;

	if (fprintf(out, "#ifdef\t__GNUC__\n"
		"#pragma GCC system_header\n"
		"#endif\n\n") < 0)
		return -1;

	while ((pvp = dt_htab_next(dtp->dt_provs, &it)) != NULL) {
		if (dt_header_provider(dtp, pvp, out) != 0) {
			dt_htab_next_destroy(it);
			return -1; /* dt_errno is set for us */
		}
	}

	if (fprintf(out, "\n#ifdef\t__cplusplus\n}\n#endif\n") < 0)
		return dt_set_errno(dtp, errno);

	if (fname != NULL && fprintf(out, "\n#endif\t/* _%s */\n", mfname) < 0)
		return dt_set_errno(dtp, errno);

	return 0;
}
