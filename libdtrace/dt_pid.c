/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2020, Oracle and/or its affiliates. All rights reserved.
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

#include <mutex.h>
#include <port.h>

#include <dt_impl.h>
#include <dt_program.h>
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
	uintptr_t dpp_pc;
	size_t dpp_size;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
	uint64_t dpp_stret[4];
	GElf_Sym dpp_last;
	uint_t dpp_last_taken;
} dt_pid_probe_t;

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
    fasttrap_probe_spec_t *ftp, const GElf_Sym *symp,
    fasttrap_probe_type_t type)
{
	ftp->ftps_type = type;
	ftp->ftps_pc = (uintptr_t)symp->st_value;
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_glen = 0;		/* no glob pattern */
	ftp->ftps_gstr[0] = '\0';

	/* Create a probe using 'ftp'. */

	return 1;
}

static int
dt_pid_create_glob_offset_probes(struct ps_prochandle *P, dtrace_hdl_t *dtp,
    fasttrap_probe_spec_t *ftp, const GElf_Sym *symp, const char *pattern)
{
	ftp->ftps_type = DTFTP_OFFSETS;
	ftp->ftps_pc = (uintptr_t)symp->st_value;
	ftp->ftps_size = (size_t)symp->st_size;
	ftp->ftps_glen = strlen(pattern);

	strncpy(ftp->ftps_gstr, pattern, ftp->ftps_glen + 1);

	/* Create a probe using 'ftp'. */

	return 1;
}

static int
dt_pid_per_sym(dt_pid_probe_t *pp, const GElf_Sym *symp, const char *func)
{
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	fasttrap_probe_spec_t *ftp;
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

	sz = sizeof(fasttrap_probe_spec_t) + strlen(pp->dpp_name);

	if ((ftp = dt_zalloc(dtp, sz)) == NULL) {
		dt_dprintf("proc_per_sym: dt_alloc(%lu) failed\n", sz);
		return 1; /* errno is set for us */
	}

	ftp->ftps_pid = pid;
	strcpy_safe(ftp->ftps_func, sizeof(ftp->ftps_func), func);

	ftp->ftps_mod = dt_pid_objname(pp->dpp_lmid, pp->dpp_obj);

	if (!isdash && gmatch("return", pp->dpp_name)) {
		if (dt_pid_create_fbt_probe(pp->dpp_pr, dtp, ftp, symp,
		    DTFTP_RETURN) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create return probe for '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	if (!isdash && gmatch("entry", pp->dpp_name)) {
		if (dt_pid_create_fbt_probe(pp->dpp_pr, dtp, ftp, symp,
		    DTFTP_ENTRY) < 0) {
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
				(u_longlong_t)off, func);
			goto out;
		}

		if (dt_pid_create_glob_offset_probes(pp->dpp_pr, pp->dpp_dtp,
					ftp, symp, pp->dpp_name) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create probes at '%s+0x%llx': %s",
				func, (u_longlong_t)off,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	} else if (glob && !isdash) {
		if (dt_pid_create_glob_offset_probes(pp->dpp_pr, pp->dpp_dtp,
					ftp, symp, pp->dpp_name) < 0) {
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
	free(ftp->ftps_mod);
	dt_free(dtp, ftp);
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
		 * Due to 4524008, _init and _fini may have a bloated st_size.
		 * While this bug has been fixed for a while, old binaries
		 * may exist that still exhibit this problem. As a result, we
		 * don't match _init and _fini though we allow users to
		 * specify them explicitly.
		 */
		if (strcmp(func, "_init") == 0 || strcmp(func, "_fini") == 0)
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

	/*
	 * Note: if an execve() happens in the victim after this point, the
	 * following lookups will (unavoidably) fail if the lmid in the previous
	 * executable is not valid iun the new one.
	 */

	if ((pp->dpp_obj = strrchr(obj, '/')) == NULL)
		pp->dpp_obj = obj;
	else
		pp->dpp_obj++;

	if (dt_Pxlookup_by_name(pp->dpp_dtp, pid, pp->dpp_lmid, obj, ".stret1",
				&sym, NULL) == 0)
		pp->dpp_stret[0] = sym.st_value;
	else
		pp->dpp_stret[0] = 0;

	if (dt_Pxlookup_by_name(pp->dpp_dtp, pid, pp->dpp_lmid, obj, ".stret2",
				&sym, NULL) == 0)
		pp->dpp_stret[1] = sym.st_value;
	else
		pp->dpp_stret[1] = 0;

	if (dt_Pxlookup_by_name(pp->dpp_dtp, pid, pp->dpp_lmid, obj, ".stret4",
					&sym, NULL) == 0)
		pp->dpp_stret[2] = sym.st_value;
	else
		pp->dpp_stret[2] = 0;

	if (dt_Pxlookup_by_name(pp->dpp_dtp, pid, pp->dpp_lmid, obj, ".stret8",
				&sym, NULL) == 0)
		pp->dpp_stret[3] = sym.st_value;
	else
		pp->dpp_stret[3] = 0;

	dt_dprintf("%s stret %llx %llx %llx %llx\n", obj,
		   (u_longlong_t)pp->dpp_stret[0],
		   (u_longlong_t)pp->dpp_stret[1],
		   (u_longlong_t)pp->dpp_stret[2],
		   (u_longlong_t)pp->dpp_stret[3]);

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
dt_pid_fix_mod(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, pid_t pid)
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
		if ((pmp = dt_pid_fix_mod(pdp, dtp, pid)) != NULL) {
			if ((obj = strchr(pdp->mod, '`')) == NULL)
				obj = pdp->mod;
			else
				obj++;

			ret = dt_pid_per_mod(&pp, pmp, obj);
		}
	}

	return ret;
}

#if 0
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
	 * The symbol ___SUNW_dof is for lazy-loaded DOF sections, and
	 * __SUNW_dof is for actively-loaded DOF sections. We try to force
	 * in both types of DOF section since the process may not yet have
	 * run the code to instantiate these providers.
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

static int
dt_pid_create_usdt_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
    dt_pcb_t *pcb, dt_proc_t *dpr)
{
	int ret = 0;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	if (dt_Pobject_iter(dtp, dpr->dpr_pid, dt_pid_usdt_mapping, dpr) != 0) {
		ret = -1;
		dt_pid_error(dtp, pcb, dpr, D_PROC_USDT,
		    "failed to instantiate probes for pid %d: %s",
		    dpr->dpr_pid, strerror(errno));
	}

	/*
	 * Put the module name in its canonical form.
	 */
	dt_pid_fix_mod(pdp, dtp, dpr->dpr_pid);

	return ret;
}
#endif

static pid_t
dt_pid_get_pid(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb,
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
		pid = dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING);
		if (pid < 0) {
			dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
			    "failed to grab process %d", (int)pid);
			return -1;
		}

		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);

		if ((err = dt_pid_create_pid_probes(pdp, dtp, pcb, dpr)) == 0) {
			/*
			 * Alert other retained enablings which may match
			 * against the newly created probes.
			 */
			dt_ioctl(dtp, DTRACEIOC_ENABLE, NULL);
		}

		dt_proc_release_unlock(dtp, pid);
	}

	/*
	 * If it's not strictly a pid provider, we might match a USDT provider.
	 */
	if (strcmp(provname, pdp->prv) != 0) {
		pid = dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING);
		if (pid < 0) {
			dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
			    "failed to grab process %d", (int)pid);
			return -1;
		}

		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);

#ifdef FIXME
		if (!dpr->dpr_usdt) {
			err = dt_pid_create_usdt_probes(pdp, dtp, pcb, dpr);
			dpr->dpr_usdt = B_TRUE;
		}
#endif

		dt_proc_release_unlock(dtp, pid);
	}

	return err ? -1 : 0;
}

int
dt_pid_create_probes_module(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	dtrace_prog_t *pgp;
	dt_stmt_t *stp;
	dtrace_probedesc_t *pdp, pd;
	pid_t pid;
	int ret = 0, found = B_FALSE;
	char provname[DTRACE_PROVNAMELEN];

	snprintf(provname, sizeof(provname), "pid%d", (int)dpr->dpr_pid);

	for (pgp = dt_list_next(&dtp->dt_programs); pgp != NULL;
	    pgp = dt_list_next(pgp)) {

		for (stp = dt_list_next(&pgp->dp_stmts); stp != NULL;
		    stp = dt_list_next(stp)) {

			pdp = &stp->ds_desc->dtsd_ecbdesc->dted_probe;
			pid = dt_pid_get_pid(pdp, dtp, NULL, dpr);
			if (pid != dpr->dpr_pid)
				continue;

			found = B_TRUE;

			pd = *pdp;

			if (gmatch(provname, pdp->prv) != 0 &&
			    dt_pid_create_pid_probes(&pd, dtp, NULL, dpr) != 0)
				ret = 1;

#ifdef FIXME
			/*
			 * If it's not strictly a pid provider, we might match
			 * a USDT provider.
			 */
			if (strcmp(provname, pdp->prv) != 0 &&
			    dt_pid_create_usdt_probes(&pd, dtp, NULL, dpr) != 0)
				ret = 1;
#endif
		}
	}

	/*
	 * Give DTrace a shot to the ribs to get it to check
	 * out the newly created probes.
	 */
	if (found)
		dt_ioctl(dtp, DTRACEIOC_ENABLE, NULL);

	return ret;
}
