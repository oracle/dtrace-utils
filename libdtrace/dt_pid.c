/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <libgen.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>

#include <mutex.h>
#include <port.h>
#include <uprobes.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_pid.h>
#include <dt_string.h>

typedef struct dt_pid_probe {
	dtrace_hdl_t *dpp_dtp;
	dt_pcb_t *dpp_pcb;
	dt_proc_t *dpp_dpr;
	struct ps_prochandle *dpp_pr;
	const char *dpp_mod;
	const char *dpp_func;
	const char *dpp_name;
	const char *dpp_obj;
	dev_t dpp_dev;
	ino_t dpp_inum;
	const char *dpp_fname;
	uintptr_t dpp_pc;
	uintptr_t dpp_vaddr;
	size_t dpp_size;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
	GElf_Sym dpp_last;
	uint_t dpp_last_taken;
} dt_pid_probe_t;

static char *dt_pid_de_pid(char *buf, const char *prv);

/*
 * Compose the lmid and object name into the canonical representation. We
 * omit the lmid for the default link map for convenience.
 */
static char *
dt_pid_objname(Lmid_t lmid, const char *obj)
{
	char *buf;
	int len;

	if (lmid == LM_ID_BASE)
		return strdup(obj);

	len = snprintf(NULL, 0, "LM%lx`%s", lmid, obj) + 1;
	buf = malloc(len);
	if (buf)
		snprintf(buf, len, "LM%lx`%s", lmid, obj);

	return buf;
}

static int
dt_pid_error(dtrace_hdl_t *dtp, dt_pcb_t *pcb, dt_proc_t *dpr,
    dt_errtag_t tag, const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	if (pcb == NULL) {
		assert(dpr != NULL);
		len = vsnprintf(dpr->dpr_errmsg, sizeof(dpr->dpr_errmsg),
		    fmt, ap);
		assert(len >= 2);
		if (dpr->dpr_errmsg[len - 2] == '\n')
			dpr->dpr_errmsg[len - 2] = '\0';
	} else {
		dt_set_errmsg(dtp, dt_errtag(tag), pcb->pcb_region,
		    pcb->pcb_filetag, pcb->pcb_fileptr ? yylineno : 0, fmt, ap);
	}
	va_end(ap);

	return 1;
}

static int
dt_pid_create_fbt_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
    pid_probespec_t *psp, const GElf_Sym *symp, pid_probetype_t type)
{
	const dt_provider_t	*pvp;

	psp->pps_prv = "pid";
	psp->pps_type = type;
	psp->pps_off = symp->st_value - psp->pps_vaddr;
	psp->pps_size = (size_t)symp->st_size;
	psp->pps_gstr[0] = '\0';		/* no glob pattern */

	/* Make sure we have a PID provider. */
	pvp = dtp->dt_prov_pid;
	if (pvp == NULL) {
		pvp = dt_provider_lookup(dtp, psp->pps_prv);
		if (pvp == NULL)
			return 0;

		dtp->dt_prov_pid = pvp;
	}

	assert(pvp->impl != NULL && pvp->impl->provide_probe != NULL);

	/* Create a probe using 'psp'. */

	return pvp->impl->provide_probe(dtp, psp);
}

static int
dt_pid_create_glob_offset_probes(struct ps_prochandle *P, dtrace_hdl_t *dtp,
    pid_probespec_t *psp, const GElf_Sym *symp, const char *pattern)
{
	psp->pps_type = DTPPT_OFFSETS;
	psp->pps_off = symp->st_value - psp->pps_vaddr;
	psp->pps_size = (size_t)symp->st_size;

	strcpy(psp->pps_gstr, pattern);

	/* Create a probe using 'psp'. */

	return 1;
}

static int
dt_pid_per_sym(dt_pid_probe_t *pp, const GElf_Sym *symp, const char *func)
{
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	pid_probespec_t *psp;
	uint64_t off;
	char *end;
	uint_t nmatches = 0;
	ulong_t sz;
	int glob, rc = 0;
	int isdash = strcmp("-", func) == 0;
	pid_t pid;

	/*
	 * We can just use the P member directly, since the PID does not change
	 * under exec().
	 */
	pid = Pgetpid(pp->dpp_pr);

	dt_dprintf("creating probe pid%d:%s:%s:%s at %lx\n", (int)pid,
	    pp->dpp_obj, func, pp->dpp_name, symp->st_value);

	sz = sizeof(pid_probespec_t) + strlen(pp->dpp_name);
	psp = dt_zalloc(dtp, sz);
	if (psp == NULL) {
		dt_dprintf("proc_per_sym: dt_alloc(%lu) failed\n", sz);
		return 1; /* errno is set for us */
	}

	psp->pps_pid = pid;
	psp->pps_mod = dt_pid_objname(pp->dpp_lmid, pp->dpp_obj);
	psp->pps_dev = pp->dpp_dev;
	psp->pps_inum = pp->dpp_inum;
	psp->pps_fn = strdup(pp->dpp_fname);
	psp->pps_vaddr = pp->dpp_vaddr;
	strcpy_safe(psp->pps_fun, sizeof(psp->pps_fun), func);

	if (!isdash && gmatch("return", pp->dpp_name)) {
		if (dt_pid_create_fbt_probe(pp->dpp_pr, dtp, psp, symp,
		    DTPPT_RETURN) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create return probe for '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	if (!isdash && gmatch("entry", pp->dpp_name)) {
		if (dt_pid_create_fbt_probe(pp->dpp_pr, dtp, psp, symp,
		    DTPPT_ENTRY) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create entry probe for '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	glob = strisglob(pp->dpp_name);
	if (!glob && nmatches == 0) {
		off = strtoull(pp->dpp_name, &end, 16);
		if (*end != '\0') {
			rc = dt_pid_error(dtp, pcb, dpr, D_PROC_NAME,
					  "'%s' is an invalid probe name",
					  pp->dpp_name);
			goto out;
		}

		if (off >= symp->st_size) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_OFF,
				"offset 0x%llx outside of function '%s'",
				(unsigned long long)off, func);
			goto out;
		}

		if (dt_pid_create_glob_offset_probes(pp->dpp_pr, pp->dpp_dtp,
					psp, symp, pp->dpp_name) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create probes at '%s+0x%llx': %s",
				func, (unsigned long long)off,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	} else if (glob && !isdash) {
		if (dt_pid_create_glob_offset_probes(pp->dpp_pr, pp->dpp_dtp,
					psp, symp, pp->dpp_name) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create offset probes in '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	pp->dpp_nmatches += nmatches;

out:
	free(psp->pps_mod);
	free(psp->pps_fn);
	dt_free(dtp, psp);
	return rc;
}

static int
dt_pid_sym_filt(void *arg, const GElf_Sym *symp, const char *func)
{
	dt_pid_probe_t *pp = arg;

	if (symp->st_shndx == SHN_UNDEF)
		return 0;

	if (symp->st_size == 0) {
		dt_dprintf("st_size of %s is zero\n", func);
		return 0;
	}

	if (pp->dpp_last_taken == 0 ||
	    symp->st_value != pp->dpp_last.st_value ||
	    symp->st_size != pp->dpp_last.st_size) {
		/*
		 * Versioned identifiers are a problem.
		 */
		if (strchr(func, '@') != NULL)
			return 0;

		/* Compiler-generated internal identifiers are a problem. */
		if (strchr(func, '.') != NULL)
			return 0;

		if ((pp->dpp_last_taken = gmatch(func, pp->dpp_func)) != 0) {
			pp->dpp_last = *symp;
			return dt_pid_per_sym(pp, symp, func);
		}
	}

	return 0;
}

static int
dt_pid_per_mod(void *arg, const prmap_t *pmp, const char *obj)
{
	dt_pid_probe_t *pp = arg;
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	pid_t pid = Pgetpid(dpr->dpr_proc);
	GElf_Sym sym;

	if (obj == NULL)
		return 0;

	dt_Plmid(pp->dpp_dtp, pid, pmp->pr_vaddr, &pp->dpp_lmid);

	pp->dpp_dev = pmp->pr_dev;
	pp->dpp_inum = pmp->pr_inum;
	pp->dpp_vaddr = pmp->pr_file->first_segment->pr_vaddr;

	/*
	 * Note: if an execve() happens in the victim after this point, the
	 * following lookups will (unavoidably) fail if the lmid in the previous
	 * executable is not valid in the new one.
	 */

	if ((pp->dpp_obj = strrchr(obj, '/')) == NULL)
		pp->dpp_obj = obj;
	else
		pp->dpp_obj++;

	/*
	 * If pp->dpp_func contains any globbing meta-characters, we need
	 * to iterate over the symbol table and compare each function name
	 * against the pattern.
	 */
	if (!strisglob(pp->dpp_func)) {
		/*
		 * If we fail to lookup the symbol, try interpreting the
		 * function as the special "-" function that indicates that the
		 * probe name should be interpreted as a absolute virtual
		 * address. If that fails and we were matching a specific
		 * function in a specific module, report the error, otherwise
		 * just fail silently in the hopes that some other object will
		 * contain the desired symbol.
		 */
		if (dt_Pxlookup_by_name(pp->dpp_dtp, pid, pp->dpp_lmid, obj,
					pp->dpp_func, &sym, NULL) != 0) {
			if (strcmp("-", pp->dpp_func) == 0) {
				sym.st_name = 0;
				sym.st_info =
				    GELF_ST_INFO(STB_LOCAL, STT_FUNC);
				sym.st_other = 0;
				sym.st_value = 0;
				sym.st_size = Pelf64(pp->dpp_pr) ? -1ULL : -1U;
			} else if (!strisglob(pp->dpp_mod)) {
				return dt_pid_error(
					dtp, pcb, dpr, D_PROC_FUNC,
					"failed to lookup '%s' in module '%s'",
					pp->dpp_func, pp->dpp_mod);
			} else
				return 0;
		}

		/*
		 * Only match defined functions of non-zero size.
		 */
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC ||
		    sym.st_shndx == SHN_UNDEF || sym.st_size == 0)
			return 0;

		/*
		 * We don't instrument writable mappings such as PLTs -- they're
		 * dynamically rewritten, and, so, inherently dicey to
		 * instrument.
		 */
		if (dt_Pwritable_mapping(pp->dpp_dtp, pid, sym.st_value))
			return 0;

		dt_Plookup_by_addr(pp->dpp_dtp, pid, sym.st_value,
				   &pp->dpp_func, &sym);

		return dt_pid_per_sym(pp, &sym, pp->dpp_func);
	} else {
		uint_t nmatches = pp->dpp_nmatches;

		if (dt_Psymbol_iter_by_addr(pp->dpp_dtp, pid, obj, PR_SYMTAB,
					    BIND_ANY | TYPE_FUNC,
					    dt_pid_sym_filt, pp) == 1)
			return 1;

		if (nmatches == pp->dpp_nmatches) {
			/*
			 * If we didn't match anything in the PR_SYMTAB, try
			 * the PR_DYNSYM.
			 */
			if (dt_Psymbol_iter_by_addr(
					pp->dpp_dtp, pid, obj,
					PR_DYNSYM, BIND_ANY | TYPE_FUNC,
					dt_pid_sym_filt, pp) == 1)
				return 1;
		}
	}

	return 0;
}

static int
dt_pid_mod_filt(void *arg, const prmap_t *pmp, const char *obj)
{
	char *name;
	dt_pid_probe_t *pp = arg;
	dt_proc_t *dpr = pp->dpp_dpr;
	int rc;

	pp->dpp_fname = obj;
	if ((pp->dpp_obj = strrchr(obj, '/')) == NULL)
		pp->dpp_obj = obj;
	else
		pp->dpp_obj++;

	if (gmatch(pp->dpp_obj, pp->dpp_mod))
		return dt_pid_per_mod(pp, pmp, obj);

	dt_Plmid(pp->dpp_dtp, Pgetpid(dpr->dpr_proc), pmp->pr_vaddr,
		 &pp->dpp_lmid);

	name = dt_pid_objname(pp->dpp_lmid, pp->dpp_obj);
	rc = gmatch(name, pp->dpp_mod);
	free(name);

	if (rc)
		return dt_pid_per_mod(pp, pmp, obj);

	return 0;
}

static const prmap_t *
dt_pid_fix_mod(dt_pid_probe_t *pp, dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
	       pid_t pid)
{
	char m[PATH_MAX];
	Lmid_t lmid = PR_LMID_EVERY;
	const char *obj;
	const prmap_t *pmp;

	/*
	 * Pick apart the link map from the library name.
	 */
	if (strchr(pdp->mod, '`') != NULL) {
		char *end;

		if (strlen(pdp->mod) < 3 || strncmp(pdp->mod, "LM", 2) != 0 ||
		    !isdigit(pdp->mod[2]))
			return NULL;

		lmid = strtoul(&pdp->mod[2], &end, 16);

		obj = end + 1;

		if (*end != '`' || strchr(obj, '`') != NULL)
			return NULL;

	} else
		obj = pdp->mod;

	if ((pmp = dt_Plmid_to_map(dtp, pid, lmid, obj)) == NULL)
		return NULL;

	dt_Pobjname(dtp, pid, pmp->pr_vaddr, m, sizeof(m));
	if (pp)
		pp->dpp_fname = strdup(m);
	if ((obj = strrchr(m, '/')) == NULL)
		obj = &m[0];
	else
		obj++;

	dt_Plmid(dtp, pid, pmp->pr_vaddr, &lmid);
	pdp->mod = dt_pid_objname(lmid, obj);

	return pmp;
}

static int
dt_pid_create_pid_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
			 dt_pcb_t *pcb, dt_proc_t *dpr)
{
	dt_pid_probe_t pp;
	int ret = 0;
	pid_t pid = Pgetpid(dpr->dpr_proc);

	pp.dpp_dtp = dtp;
	pp.dpp_dpr = dpr;
	pp.dpp_pr = dpr->dpr_proc;
	pp.dpp_pcb = pcb;
	pp.dpp_nmatches = 0;
	pp.dpp_dev = makedev(0, 0);
	pp.dpp_inum = 0;

	/*
	 * Prohibit self-grabs.  (This is banned anyway by libproc, but this way
	 * we get a nicer error message.)
	 */
	if (pid == getpid())
		return dt_pid_error(dtp, pcb, dpr, D_PROC_DYN,
				    "process %s is dtrace itself",
				    &pdp->prv[3]);

	/*
	 * We can only trace dynamically-linked executables (since we've
	 * hidden some magic in ld.so.1 as well as libc.so.1).
	 */
	if (dt_Pname_to_map(dtp, pid, PR_OBJ_LDSO) == NULL) {
		return dt_pid_error(dtp, pcb, dpr, D_PROC_DYN,
			"process %s is not a dynamically-linked executable",
			&pdp->prv[3]);
	}

	pp.dpp_mod = pdp->mod[0] != '\0' ? pdp->mod : "*";
	pp.dpp_func = pdp->fun[0] != '\0' ? pdp->fun : "*";
	pp.dpp_name = pdp->prb[0] != '\0' ? pdp->prb : "*";
	pp.dpp_last_taken = 0;
	pp.dpp_fname = NULL;

	if (strcmp(pp.dpp_func, "-") == 0) {
		const prmap_t *aout, *pmp;

		if (pdp->mod[0] == '\0') {
			pp.dpp_mod = pdp->mod;
			pdp->mod = "a.out";
		} else if (strisglob(pp.dpp_mod) ||
		    (aout = dt_Pname_to_map(dtp, pid, "a.out")) == NULL ||
		    (pmp = dt_Pname_to_map(dtp, pid, pp.dpp_mod)) == NULL ||
		    aout->pr_vaddr != pmp->pr_vaddr) {
			return dt_pid_error(dtp, pcb, dpr, D_PROC_LIB,
			    "only the a.out module is valid with the "
			    "'-' function");
		}

		if (strisglob(pp.dpp_name)) {
			return dt_pid_error(dtp, pcb, dpr, D_PROC_NAME,
			    "only individual addresses may be specified "
			    "with the '-' function");
		}
	}

	/*
	 * If pp.dpp_mod contains any globbing meta-characters, we need
	 * to iterate over each module and compare its name against the
	 * pattern. An empty module name is treated as '*'.
	 */
	if (strisglob(pp.dpp_mod)) {
		ret = dt_Pobject_iter(dtp, pid, dt_pid_mod_filt, &pp);
	} else {
		const prmap_t *pmp;
		const char *obj;

		/*
		 * If we can't find a matching module, don't sweat it -- either
		 * we'll fail the enabling because the probes don't exist or
		 * we'll wait for that module to come along.
		 */
		pmp = dt_pid_fix_mod(&pp, pdp, dtp, pid);
		if (pmp != NULL) {
			if ((obj = strchr(pdp->mod, '`')) == NULL)
				obj = pdp->mod;
			else
				obj++;

			ret = dt_pid_per_mod(&pp, pmp, obj);
		}
	}

	if (pp.dpp_func != pdp->fun) {
		free((char *)pdp->fun);
		pdp->fun = pp.dpp_func;
	}

	return ret;
}

/*
 * Rescan the PID uprobe list and create suitable underlying probes.
 *
 * If dpr is set, just set up probes relating to mappings found in that one
 * process.  (dpr must in this case be locked.)
 *
 * If pdp is set, create overlying USDT probes for the specified probe
 * description.
 *
 * Return 0 on success or -1 on error.  (Failure to create specific underlying
 * probes is not an error.)
 */
static int
dt_pid_create_usdt_probes(dtrace_hdl_t *dtp, dt_proc_t *dpr,
			  dtrace_probedesc_t *pdp, dt_pcb_t *pcb)
{
	const dt_provider_t *pvp;
	FILE *f;
	char *buf = NULL;
	size_t sz;
	int ret = 0;

	pvp = dtp->dt_prov_usdt;
	if (!pvp) {
		pvp = dt_provider_lookup(dtp, "usdt");
		assert(pvp != NULL);
		dtp->dt_prov_usdt = pvp;
	}

	assert(pvp->impl != NULL && pvp->impl->provide_probe != NULL);

	f = fopen(TRACEFS "uprobe_events", "r");
	if (!f) {
		dt_dprintf("cannot open " TRACEFS "uprobe_events: %s\n",
		    strerror(errno));
		return -1;
	}

	/*
	 * Systemwide probing: not yet implemented.
	 */
	assert(dpr != NULL);

	dt_dprintf("Scanning for usdt probes matching %i\n", dpr->dpr_pid);

	/*
	 * We are only interested in pid uprobes, not any other uprobes that may
	 * exist.  Some of these may be for pid probes, some for usdt: we create
	 * underlying probes for all of them, except that we may only be
	 * interested in probes belonging to mappings in a particular process,
	 * in which case we create probes for that process only.
	 */
	while (getline(&buf, &sz, f) >= 0) {
		dev_t dev;
		ino_t inum;
		uint64_t off;
		unsigned long long dev_ll, inum_ll;
		char *inum_str = NULL;
		char *spec = NULL;
		char *eprv = NULL, *emod = NULL, *efun = NULL, *eprb = NULL;
		char *prv = NULL, *mod = NULL, *fun = NULL, *prb = NULL;
		pid_probespec_t psp;

#define UPROBE_PREFIX "p:dt_pid/p_"
		if (strncmp(buf, UPROBE_PREFIX, strlen(UPROBE_PREFIX)) != 0)
			continue;

		switch (sscanf(buf, "p:dt_pid/p_%llx_%llx_%lx %ms "
			       "P%m[^= ]=\\1 M%m[^= ]=\\2 F%m[^= ]=\\3 N%m[^= ]=\\4", &dev_ll, &inum_ll,
			       &off, &spec, &eprv, &emod, &efun, &eprb)) {
		case 8: /* Includes dtrace probe names: decode them. */
			prv = uprobe_decode_name(eprv);
			mod = uprobe_decode_name(emod);
			fun = uprobe_decode_name(efun);
			prb = uprobe_decode_name(eprb);
			dev = dev_ll;
			inum = inum_ll;
			break;
		case 4: /* No dtrace probe name - not a USDT probe. */
			goto next;
		default:
			if ((strlen(buf) > 0) && (buf[strlen(buf)-1] == '\n'))
				buf[strlen(buf)-1] = 0;
			dt_dprintf("Cannot parse %s as a DTrace uprobe name\n", buf);
			goto next;
		}

		if (asprintf(&inum_str, "%llx", inum_ll) < 0)
			goto next;

		/*
		 * Make the underlying probe, if not already present.
		 */
		memset(&psp, 0, sizeof(pid_probespec_t));

		/* FIXME: DTPPT_IS_ENABLED needs to be supported also.  */
		psp.pps_type = DTPPT_OFFSETS;

		/*
		 * These components are only used for creation of an underlying
		 * probe with no overlying counterpart: usually these are those
		 * not explicitly listed in the D program, which will never be
		 * enabled.  In future this may change.
		 */
		psp.pps_prv = prv;
		psp.pps_mod = mod;
		strcpy_safe(psp.pps_fun, sizeof(psp.pps_fun), fun);
		psp.pps_prb = prb;

		/*
		 * Always used.
		 */
		psp.pps_dev = dev;
		psp.pps_inum = inum;
		psp.pps_off = off;

		/*
		 * Filter out probes not related to the process of interest.
		 */
		if (dpr && dpr->dpr_proc) {
			assert(MUTEX_HELD(&dpr->dpr_lock));
			if (Pinode_to_file_map(dpr->dpr_proc, dev, inum) == NULL)
				goto next;
			psp.pps_pid = Pgetpid(dpr->dpr_proc);
		}

		/*
		 * Does this match the overlying probe we are meant to be
		 * creating?  If so, create an overlying probe and let
		 * provide_probe() create the underlying probe for us.  Among
		 * other things, this ensures that the PID part of the provider
		 * name gets stuck into the overlying probe properly.
		 *
		 * TODO: wildcard probes get handled here.
		 */
		if (pdp) {
			char de_pid_prov[DTRACE_PROVNAMELEN + 1];

			if ((pdp->prv[0] != 0 &&
			     strcmp(dt_pid_de_pid(de_pid_prov, pdp->prv),
				    psp.pps_prv) != 0) ||
			    (pdp->mod[0] != 0 && strcmp(pdp->mod, psp.pps_mod) != 0) ||
			    (pdp->fun[0] != 0 && strcmp(pdp->fun, psp.pps_fun) != 0) ||
			    (pdp->prb[0] != 0 && strcmp(pdp->prb, psp.pps_prb) != 0)) {
				dt_dprintf("%s:%s:%s:%s -> %s:%s:%s:%s: match failure\n",
					   pdp->prv, pdp->mod, pdp->fun, pdp->prb,
					   psp.pps_prv, psp.pps_mod, psp.pps_fun,
					   psp.pps_prb);
				goto next;
			}
		}

		/*
		 * Create a probe using psp, if not already present.
		 *
		 * If we are creating an overlying probe, complain about it if
		 * probe creation fails.  Otherwise, this is just an underlying
		 * probe: we'll complain later if we use it for anything.
		 */

		if (pvp->impl->provide_probe(dtp, &psp) < 0 && pdp) {
			dt_pid_error(dtp, pcb, dpr, D_PROC_USDT,
				     "failed to instantiate probe %s for pid %d: %s",
				     pdp->prb, dpr->dpr_pid,
				     dtrace_errmsg(dtp, dtrace_errno(dtp)));
			ret = -1;
		}

		if (pdp && dpr) {
			/*
			 * Put the module name in its canonical form.
			 */
			dt_pid_fix_mod(NULL, pdp, dtp, dpr->dpr_pid);
		}

	next:
		free(eprv); free(emod); free(efun); free(eprb);
		free(prv);  free(mod);  free(fun);  free(prb);
		free(inum_str);
		free(spec);
		continue;
	}
	free(buf);

	return ret;
}

#if 0 /* Almost certainly unnecessary in this form */
static int
dt_pid_usdt_mapping(void *data, const prmap_t *pmp, const char *oname)
{
	dt_proc_t *dpr = data;
	GElf_Sym sym;
	prsyminfo_t sip;
	dof_helper_t dh;
	GElf_Half e_type;
	const char *mname;
	const char *syms[] = { "___SUNW_dof", "__SUNW_dof" };
	int i;
	int fd = -1;

	/*
	 * We try to force-load the DOF since the process may not yet have run
	 * the code to instantiate these providers.
	 */
	for (i = 0; i < 2; i++) {
		if (dt_Pxlookup_by_name(dpr->dpr_hdl, dpr->dpr_pid, PR_LMID_EVERY,
			oname, syms[i], &sym, &sip) != 0) {
			continue;
		}

		if ((mname = strrchr(oname, '/')) == NULL)
			mname = oname;
		else
			mname++;

		dt_dprintf("lookup of %s succeeded for %s\n", syms[i], mname);

		if (dt_Pread(dpr->dpr_hdl, dpr->dpr_pid, &e_type,
			sizeof(e_type), pmp->pr_vaddr + offsetof(Elf64_Ehdr,
			    e_type)) != sizeof(e_type)) {
			dt_dprintf("read of ELF header failed");
			continue;
		}

		dh.dofhp_dof = sym.st_value;
		dh.dofhp_addr = (e_type == ET_EXEC) ? 0 : pmp->pr_vaddr;
		dh.dofhp_mod = dt_pid_objname(sip.prs_lmid, mname);
		if (fd == -1 &&
		    (fd = pr_open(dpr->dpr_proc, "/dev/dtrace/helper", O_RDWR, 0)) < 0) {
			dt_dprintf("pr_open of helper device failed: %s\n",
			    strerror(errno));
			return -1; /* errno is set for us */
		}

		if (pr_ioctl(P.P, fd, DTRACEHIOC_ADDDOF, &dh, sizeof(dh)) < 0)
			dt_dprintf("DOF was rejected for %s\n", dh.dofhp_mod);
	}
	if (fd != -1)
		pr_close(P.P, fd);

	return 0;
}
#endif

/*
 * Extract the pid from a USDT provider name.
 */
pid_t
dt_pid_get_pid(const dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb,
	       dt_proc_t *dpr)
{
	pid_t pid;
	char *end;
	const char *c, *last = NULL;

	for (c = &pdp->prv[0]; *c != '\0'; c++) {
		if (!isdigit(*c))
			last = c;
	}

	if (last == NULL || (*(++last) == '\0')) {
		dt_pid_error(dtp, pcb, dpr, D_PROC_BADPROV,
			     "'%s' is not a valid provider", pdp->prv);
		return -1;
	}

	errno = 0;
	pid = strtol(last, &end, 10);

	if (errno != 0 || end == last || end[0] != '\0' || pid <= 0) {
		dt_pid_error(dtp, pcb, dpr, D_PROC_BADPID,
			     "'%s' does not contain a valid pid", pdp->prv);
		return -1;
	}

	return pid;
}

/*
 * Return a USDT provider name with the pid removed.
 */
static char *
dt_pid_de_pid(char *buf, const char *prv)
{
	const char *c;
	char *p = buf;

	for (c = prv; *c != '\0'; c++) {
		if (!isdigit(*c))
			*p++ = *c;
	}
	*p = 0;
	return buf;
}

int
dt_pid_create_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb)
{
	char provname[DTRACE_PROVNAMELEN];
	dt_proc_t *dpr;
	pid_t pid;
	int err = 0;

	assert(pcb != NULL);

	if ((pid = dt_pid_get_pid(pdp, dtp, pcb, NULL)) == -1)
		return -1;

	snprintf(provname, sizeof(provname), "pid%d", (int)pid);

	if (gmatch(provname, pdp->prv) != 0) {
		if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING) < 0) {
			dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
			    "failed to grab process %d", (int)pid);
			return -1;
		}

		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);

		err = dt_pid_create_pid_probes(pdp, dtp, pcb, dpr);
		dt_proc_release_unlock(dtp, pid);
	}

	/*
	 * If it's not strictly a pid provider, we might match a USDT provider.
	 */
	if (strcmp(provname, pdp->prv) != 0) {
		if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING) < 0) {
			dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
			    "failed to grab process %d", (int)pid);
			return -1;
		}

		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);

		if (!dpr->dpr_usdt) {
			err = dt_pid_create_usdt_probes(dtp, dpr, pdp, pcb);
			dpr->dpr_usdt = B_TRUE;
		}

		dt_proc_release_unlock(dtp, pid);
	}

	/* (USDT systemwide probing goes here.)  */

	return err ? -1 : 0;
}

int
dt_pid_create_probes_module(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	dtrace_prog_t *pgp;
	dt_stmt_t *stp;
	dtrace_probedesc_t *pdp;
	pid_t pid;
	int ret = 0;
	char provname[DTRACE_PROVNAMELEN];

	snprintf(provname, sizeof(provname), "pid%d", (int)dpr->dpr_pid);

	for (pgp = dt_list_next(&dtp->dt_programs); pgp != NULL;
	    pgp = dt_list_next(pgp)) {
		for (stp = dt_list_next(&pgp->dp_stmts); stp != NULL;
		    stp = dt_list_next(stp)) {
			dtrace_probedesc_t	pd;

			pdp = &stp->ds_desc->dtsd_ecbdesc->dted_probe;
			pid = dt_pid_get_pid(pdp, dtp, NULL, dpr);
			if (pid != dpr->dpr_pid)
				continue;

			pd = *pdp;
			pd.fun = strdup(pd.fun);	/* we may change it */

			if (gmatch(provname, pdp->prv) != 0 &&
			    dt_pid_create_pid_probes(&pd, dtp, NULL, dpr) != 0)
				ret = 1;

			/*
			 * If it's not strictly a pid provider, we might match
			 * a USDT provider.
			 */
			if (strcmp(provname, pdp->prv) != 0 &&
			    dt_pid_create_usdt_probes(dtp, dpr, pdp, NULL) < 0)
				ret = 1;

			free((char *)pd.fun);
		}
	}

	/*
	 * XXX systemwide: rescan for new probes here?  We have to do it
	 * at some point, but when?
	 */

	return ret;
}
