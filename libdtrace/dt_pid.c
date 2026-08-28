/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <alloca.h>
#include <libgen.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#if defined(__amd64)
#include <disasm.h>
#endif

#include <port.h>
#include <usdt_parser.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_pid.h>
#include <dt_string.h>

#define SEC_STAPSDT_NOTE	".note.stapsdt"
#define NAME_STAPSDT_NOTE	"stapsdt"

/*
 * Information on a PID probe.
 */
typedef struct dt_pid_probe {
	dtrace_hdl_t *dpp_dtp;
	dt_proc_t *dpp_dpr;
	struct ps_prochandle *dpp_pr;
	const char *dpp_mod;
	const char *dpp_func;
	const char *dpp_name;
	const char *dpp_obj;
	dev_t dpp_dev;
	ino_t dpp_inum;
	const char *dpp_fname;
	uintptr_t dpp_base;
	uintptr_t dpp_vaddr;
	Lmid_t dpp_lmid;
	uint_t dpp_nmatches;
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

	if (lmid == LM_ID_BASE)
		return strdup(obj);

	if (asprintf(&buf, "LM%lx`%s", lmid, obj) < 0)
		return NULL;

	return buf;
}

static int
dt_pid_error(dtrace_hdl_t *dtp, dt_proc_t *dpr, dt_errtag_t tag,
	     const char *fmt, ...)
{
	va_list		ap;
	dt_pcb_t	*pcb = dtp->dt_pcb;

	va_start(ap, fmt);
	if (pcb != NULL) {
		dt_set_errmsg(dtp, dt_errtag(tag), pcb->pcb_region,
		    pcb->pcb_filetag, pcb->pcb_fileptr ? yylineno : 0, fmt, ap);
	} else if (dpr != NULL) {
		int	len;

		len = vsnprintf(dpr->dpr_errmsg, sizeof(dpr->dpr_errmsg),
				fmt, ap);
		assert(len >= 2);
		if (dpr->dpr_errmsg[len - 2] == '\n')
			dpr->dpr_errmsg[len - 2] = '\0';
	} else
		dt_set_errmsg(dtp, dt_errtag(tag), NULL, NULL, 0, fmt, ap);
	va_end(ap);

	return 1;
}

static int
dt_pid_create_one_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
    pid_probespec_t *psp, pid_probetype_t type)
{
	const dt_provider_t	*pvp = dtp->dt_prov_pid;

	psp->pps_type = type;
	psp->pps_prv = "pid";

	/* Make sure we have a PID provider. */
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

#if defined(__amd64)
#if defined(HAVE_INITDISINFO3) == defined(HAVE_INITDISINFO4)
#error expect init_disassemble_info() to have 3 or else 4 arguments
#endif
#ifdef HAVE_INITDISINFO4
static int
my_callback2(void *stream, enum disassembler_style style, const char *fmt, ...) {
	return 0;
}
#endif
static int
my_callback(void *stream, const char *fmt, ...)
{
	return 0;
}
#endif

static int
dt_pid_per_sym(dt_pid_probe_t *pp, const GElf_Sym *symp, const char *func)
{
	dtrace_hdl_t *dtp = pp->dpp_dtp;
	dt_proc_t *dpr = pp->dpp_dpr;
	pid_probespec_t *psp;
	uint64_t off;
	uint_t nmatches = 0;
	ulong_t sz;
	int glob, rc = 0;
	pid_t pid;

	/*
	 * We can just use the P member directly, since the PID does not change
	 * under exec().
	 */
	pid = Pgetpid(pp->dpp_pr);

	dt_dprintf("creating probe pid%d:%s:%s:%s at %lx\n", (int)pid,
	    pp->dpp_obj, func, pp->dpp_name, symp->st_value);

	sz = sizeof(pid_probespec_t);
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
	psp->pps_fun = (char *) func;
	psp->pps_nameoff = 0;
	psp->pps_off = symp->st_value - pp->dpp_vaddr;

	/*
	 * The special function "-" means the probe name is an absolute
	 * virtual address.
	 */
	if (strcmp("-", func) == 0) {
		char *end;
		GElf_Sym sym;

		off = strtoull(pp->dpp_name, &end, 16);
		if (*end != '\0') {
			rc = dt_pid_error(dtp, dpr, D_PROC_NAME,
					  "'%s' is an invalid probe name",
					  pp->dpp_name);
			goto out;
		}

		psp->pps_nameoff = off;
		off += pp->dpp_base;

		if (dt_Plookup_by_addr(dtp, pid, off, (const char **)&psp->pps_fun, &sym)) {
			rc = dt_pid_error(dtp, dpr, D_PROC_NAME,
				"failed to lookup 0x%lx in module '%s'",
				off, pp->dpp_mod);
			if (psp->pps_fun != func && psp->pps_fun != NULL)
				free(psp->pps_fun);
			goto out;
		}

		psp->pps_prb = (char*)pp->dpp_name;
		psp->pps_off = off - pp->dpp_vaddr;

		if (dt_pid_create_one_probe(pp->dpp_pr, dtp, psp, DTPPT_ABSOFFSETS) < 0)
			rc = dt_pid_error(dtp, dpr, D_PROC_CREATEFAIL,
				"failed to create probes at '%s+0x%llx': %s",
				func, (unsigned long long)off,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
		else
			pp->dpp_nmatches++;
		free(psp->pps_fun);
		goto out;
	}

	if (gmatch("return", pp->dpp_name)) {
		if (dt_pid_create_one_probe(pp->dpp_pr, dtp, psp, DTPPT_RETURN) < 0) {
			rc = dt_pid_error(dtp, dpr, D_PROC_CREATEFAIL,
				"failed to create return probe for '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	if (gmatch("entry", pp->dpp_name)) {
		if (dt_pid_create_one_probe(pp->dpp_pr, dtp, psp, DTPPT_ENTRY) < 0) {
			rc = dt_pid_error(dtp, dpr, D_PROC_CREATEFAIL,
				"failed to create entry probe for '%s': %s",
				func, dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	}

	glob = strisglob(pp->dpp_name);
	if (!glob && nmatches == 0) {
		char *end;

		off = strtoull(pp->dpp_name, &end, 16);
		if (*end != '\0') {
			rc = dt_pid_error(dtp, dpr, D_PROC_NAME,
					  "'%s' is an invalid probe name",
					  pp->dpp_name);
			goto out;
		}

		if (off >= symp->st_size) {
			rc = dt_pid_error(dtp, dpr, D_PROC_OFF,
				"offset 0x%llx outside of function '%s'",
				(unsigned long long)off, func);
			goto out;
		}

		psp->pps_nameoff = off;
		psp->pps_off = symp->st_value - pp->dpp_vaddr + off;
		if (dt_pid_create_one_probe(pp->dpp_pr, dtp,
					psp, DTPPT_OFFSETS) < 0) {
			rc = dt_pid_error(dtp, dpr, D_PROC_CREATEFAIL,
				"failed to create probes at '%s+0x%llx': %s",
				func, (unsigned long long)off,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	} else if (glob) {
#if defined(__amd64)
		/*
		 * We need to step through the instructions to find their
		 * offsets.  This is difficult on x86, which has variable
		 * instruction lengths.  We invoke the disassembler in
		 * libopcodes.
		 *
		 * We look for the Elf pointer.  It is already stored in
		 * file_elf in file_info_t, but getting it back over here
		 * means introducing new struct members, new arguments to
		 * functions, etc.  So just call elf_begin() again here.
		 */
		int fd, i;
		Elf *elf;
		Elf_Scn *scn = NULL;
		GElf_Sym sym;
		GElf_Shdr shdr;
		Elf_Data *data;
		size_t shstrndx, off;
		disassembler_ftype disasm;

		/* Set things up. */
		fd = open(pp->dpp_fname, O_RDONLY);
		elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);   // ELF_C_READ ?
		assert(elf_kind(elf) == ELF_K_ELF);
		elf_getshdrstrndx(elf, &shstrndx);

		/* Look for the symbol table. */
		while (1) {
			scn = elf_nextscn(elf, scn);
			if (scn == NULL)
				goto out;
			assert(gelf_getshdr(scn, &shdr) != NULL);
			if (shdr.sh_type == SHT_SYMTAB)
				break;
		}

		/* Look for the symbol in the symbol table. */
		data = elf_getdata(scn, NULL);
		for (i = 0; i < data->d_size / sizeof(GElf_Sym); i++) {
			if (!gelf_getsym(data, i, &sym))
				continue;
			if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL)
				continue;
			if (strcmp(elf_strptr(elf, shdr.sh_link, sym.st_name), func) == 0)
				break;
		}
		if (i >= data->d_size / sizeof(GElf_Sym))
			goto out;

		/* Get the section for our symbol. */
		scn = elf_getscn(elf, sym.st_shndx);
		assert(gelf_getshdr(scn, &shdr) != NULL);

		/* Check that the section is text. */
		if (shdr.sh_type != SHT_PROGBITS ||
		    shdr.sh_size <= 0 ||
		    (shdr.sh_flags & SHF_EXECINSTR) == 0) {
			assert(0);
		}
		assert(strcmp(elf_strptr(elf, shstrndx, shdr.sh_name), ".text") == 0);

		/* Get the instructions. */
		data = elf_getdata(scn, NULL);

		/*
		 * "Disassemble" instructions just to get the offsets.
		 *
		 * Unfortunately, libopcodes's disassembler() has a different
		 * interface in binutils versions before 2.29.
		 */
#if defined(HAVE_DIS1) == defined(HAVE_DIS4)
#error expect disassembler() to have 1 or else 4 arguments
#endif
#ifdef HAVE_DIS1
		bfd			*abfd;
		struct disassemble_info	disasm_info;

		bfd_init();
		abfd = bfd_openr(pp->dpp_fname, NULL);
		if (!bfd_check_format(abfd, bfd_object))
			return 1;

		disasm = disassembler(abfd);
#else
		disassemble_info disasm_info;

		disasm = disassembler(bfd_arch_i386, false, bfd_mach_x86_64, NULL);
#endif
#ifdef HAVE_INITDISINFO4
		init_disassemble_info(&disasm_info, NULL, my_callback, my_callback2);
#else
		init_disassemble_info(&disasm_info, NULL, my_callback);
#endif
		disasm_info.buffer = data->d_buf + (sym.st_value - shdr.sh_addr);
		disasm_info.buffer_length = sym.st_size;
#else
		/*
		 * The situation on aarch64 is much simpler:  each instruction
		 * is 4 bytes.
		 */
#define disasm(x, y) 4
#endif

		psp->pps_flags |= DT_PID_PSP_FLAG_OPTIONAL;
		for (off = 0; off < symp->st_size; off += disasm(off, &disasm_info)) {
			char offstr[32];

			snprintf(offstr, sizeof(offstr), "%lx", off);
			if (!gmatch(offstr, pp->dpp_name))
				continue;

			psp->pps_nameoff = off;
			psp->pps_off = symp->st_value - pp->dpp_vaddr + off;
			if (dt_pid_create_one_probe(pp->dpp_pr, dtp,
						psp, DTPPT_OFFSETS) >= 0)
				nmatches++;
		}

#if defined(__amd64)
		/* Shut things down. */
		elf_end(elf);
		close(fd);
#ifdef HAVE_DIS1
		bfd_close(abfd);
#endif
#endif
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
	dt_proc_t *dpr = pp->dpp_dpr;
	pid_t pid = Pgetpid(dpr->dpr_proc);
	GElf_Sym sym;

	if (obj == NULL)
		return 0;

	dt_Plmid(dtp, pid, pmp->pr_vaddr, &pp->dpp_lmid);

	pp->dpp_dev = pmp->pr_dev;
	pp->dpp_inum = pmp->pr_inum;
	pp->dpp_vaddr = pmp->pr_file->first_segment->pr_vaddr;
	pp->dpp_base = (pmp->pr_mflags & MA_PIC) ? pp->dpp_vaddr : 0;

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
	 * If it is the special function "-", cut to dt_pid_per_sym() now.
	 */
	if (strcmp("-", pp->dpp_func) == 0)
		return dt_pid_per_sym(pp, &sym, pp->dpp_func);

	/*
	 * If pp->dpp_func contains any globbing meta-characters, we need
	 * to iterate over the symbol table and compare each function name
	 * against the pattern.
	 */
	if (!strisglob(pp->dpp_func)) {
		/* If we are matching a specific function in a specific module,
		 * report the error, otherwise just fail silently in the hopes
		 * that some other object will contain the desired symbol.
		 */
		if (dt_Pxlookup_by_name(dtp, pid, pp->dpp_lmid, obj,
					pp->dpp_func, &sym, NULL) != 0) {
			if (!strisglob(pp->dpp_mod)) {
				return dt_pid_error(dtp, dpr, D_PROC_FUNC,
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
		if (dt_Pwritable_mapping(dtp, pid, sym.st_value))
			return 0;

		dt_Plookup_by_addr(dtp, pid, sym.st_value,
				   &pp->dpp_func, &sym);

		return dt_pid_per_sym(pp, &sym, pp->dpp_func);
	} else {
		uint_t nmatches = pp->dpp_nmatches;

		if (dt_Psymbol_iter_by_addr(dtp, pid, obj, PR_SYMTAB,
					    BIND_ANY | TYPE_FUNC,
					    dt_pid_sym_filt, pp) == 1)
			return 1;

		if (nmatches == pp->dpp_nmatches) {
			/*
			 * If we didn't match anything in the PR_SYMTAB, try
			 * the PR_DYNSYM.
			 */
			if (dt_Psymbol_iter_by_addr(dtp, pid, obj,
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

/*
 * Create pid probes for the specified process.
 */
static int
dt_pid_create_pid_probes_proc(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
			      dt_proc_t *dpr)
{
	dt_pid_probe_t pp;
	int ret = 0;
	pid_t pid = Pgetpid(dpr->dpr_proc);

	pp.dpp_dtp = dtp;
	pp.dpp_dpr = dpr;
	pp.dpp_pr = dpr->dpr_proc;
	pp.dpp_nmatches = 0;
	pp.dpp_dev = makedev(0, 0);
	pp.dpp_inum = 0;

	/*
	 * Prohibit self-grabs.  (This is banned anyway by libproc, but this way
	 * we get a nicer error message.)
	 */
	if (pid == getpid())
		return dt_pid_error(dtp, dpr, D_PROC_DYN,
				    "process %s is dtrace itself",
				    &pdp->prv[3]);

	/*
	 * We can only trace dynamically-linked executables (since we've
	 * hidden some magic in ld.so.1 as well as libc.so.1).
	 */
	if (dt_Pname_to_map(dtp, pid, PR_OBJ_LDSO) == NULL) {
		return dt_pid_error(dtp, dpr, D_PROC_DYN,
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

		if (strcmp(pp.dpp_mod, "*") == 0) {
			/* Tolerate two glob cases:  "" and "*". */
			pdp->mod = "a.out";
			pp.dpp_mod = pdp->mod;
		} else if (strisglob(pp.dpp_mod) ||
		    (aout = dt_Pname_to_map(dtp, pid, "a.out")) == NULL ||
		    (pmp = dt_Pname_to_map(dtp, pid, pp.dpp_mod)) == NULL ||
		    aout->pr_vaddr != pmp->pr_vaddr) {
			return dt_pid_error(dtp, dpr, D_PROC_LIB,
				"only the a.out module is valid with the "
				"'-' function");
		}

		if (strisglob(pp.dpp_name)) {
			return dt_pid_error(dtp, dpr, D_PROC_NAME,
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
 * Read a file into a buffer and return it.
 */
static void *
read_file(const char *name, size_t *size)
{
	int fd;
	struct stat s;
	char *buf = NULL;
	char *bufptr;
	int len;

	if ((fd = open(name, O_RDONLY | O_CLOEXEC)) < 0) {
		dt_dprintf("cannot open %s while scanning for USDT DOF: %s\n",
			   name, strerror(errno));
		return NULL;
	}

	if (fstat(fd, &s) < 0) {
		dt_dprintf("cannot stat while scanning for USDT DOF: %s\n",
			   strerror(errno));
		goto err;
	}
	if ((buf = malloc(s.st_size)) == NULL) {
		dt_dprintf("Out of memory allocating %zi bytes while scanning for USDT DOF\n",
			   s.st_size);
		goto err;
	}
	*size = s.st_size;

	bufptr = buf;
	while ((len = read(fd, bufptr, s.st_size)) < s.st_size) {
		if (len < 0) {
			if (errno != EINTR) {
				dt_dprintf("Cannot read USDT DOF: %s\n",
					   strerror(errno));
				goto err;
			}
			continue;
		}
		s.st_size -= len;
		bufptr += len;
	}
	close(fd);
	return buf;
err:
	free(buf);
	close(fd);
	return NULL;
}

/*
 * Ensure that the buffer has enough data to read the record of the expected
 * type.  Ensure that all records have at least 1 byte of payload data.
 */
static int
validate_dof_record(const char *path, const dof_parsed_t *parsed,
		    dof_parsed_info_t type, size_t headsz, size_t buf_size,
		    size_t seen_size)
{
	size_t	data_size;

	/* If we have read more than there is, we must always fail. */
	if (buf_size < seen_size)
		data_size = 0;
	else
		data_size = buf_size - seen_size;

	if (data_size < headsz || data_size < parsed->size) {
		dt_dprintf("DOF too small when adding probes (seen %zi bytes)\n",
			   seen_size);
		return 0;
	}

	if (parsed->size < headsz + 1) {
		dt_dprintf("DOF record too small: expected %zi, got %zi\n",
			   headsz, parsed->size);
		return 0;
	}

	if (parsed->type != type) {
		dt_dprintf("%s format invalid: expected %i, got %i\n", path,
			   type, parsed->type);
		return 0;
	}

	return 1;
}

/*
 * Validate a payload containing CNT consecutive NUL-terminated strings.
 */
static int
validate_string_payload(const char *path, const char *payload,
			size_t payload_size, size_t cnt)
{
	const char	*p = payload;
	const char	*end = payload + payload_size;
	size_t		i;

	for (i = 0; i < cnt; i++) {
		const char	*nul;

		if (p >= end) {
			dt_dprintf("%s string payload too small\n", path);
			return 0;
		}

		nul = memchr(p, '\0', end - p);
		if (nul == NULL) {
			dt_dprintf("%s string payload unterminated\n", path);
			return 0;
		}

		p = nul + 1;
	}

	return 1;
}

static int
validate_argmap_payload(const char *path, const uint8_t *argmap,
			size_t payload_size, size_t nargc, size_t xargc)
{
	size_t	i;

	if (payload_size < xargc * sizeof(uint8_t)) {
		dt_dprintf("%s argmap payload too small\n", path);
		return 0;
	}

	for (i = 0; i < xargc; i++) {
		if (argmap[i] >= nargc) {
			dt_dprintf("%s argmap entry %zi invalid: %u >= %zi\n",
				   path, i, argmap[i], nargc);
			return 0;
		}
	}

	return 1;
}


/*
 * Create underlying probes relating to the probe description passed on input.
 * Just set up probes relating to mappings found in this one process.
 *
 * Either the pid must be specified or else dpr must be set and locked.
 *
 * Return 0 on success or -1 on error.  (Failure to create specific underlying
 * probes is not an error.)
 */
static int
dt_pid_create_usdt_probes_proc(dtrace_hdl_t *dtp, pid_t pid, dt_proc_t *dpr,
			       dtrace_probedesc_t *pdp)
{
	const dt_provider_t *pvp;
	int ret = 0;
	int dpr_caller;		/* dpr was set by caller */
	char *probepath = NULL;
	glob_t probeglob = {0};

	if (dpr == NULL) {
		assert(pid != -1);
		dpr_caller = 0;
	} else {
		assert(pid == -1);
		assert(dpr->dpr_proc);
		assert(MUTEX_HELD(&dpr->dpr_lock));
		pid = dpr->dpr_pid;
		dpr_caller = 1;
	}

	dt_dprintf("Scanning for usdt probes in %i matching %s:%s:%s\n",
		   pid, pdp->mod, pdp->fun, pdp->prb);

	pvp = dt_provider_lookup(dtp, "usdt");
	assert(pvp != NULL);

	if (dpr != NULL && Pstate(dpr->dpr_proc) == PS_DEAD)
		return 0;

	/*
	 * Look for DOF matching this probe in the global probe DOF stash, in
	 * /run/dtrace/probes/$pid/$pid$prv/$mod/$fun/$prb: glob expansion means
	 * that this may relate to multiple probes.  (This is why we retain
	 * a run-together $pid$prv component, because the glob may match text on
	 * both sides of the boundary between $pid and $prv.)
	 *
	 * Using this is safe because the parsed DOF is guaranteed up to date
	 * with the current DTrace, being reparsed by the currently-running
	 * daemon, and was parsed in a seccomp jail.  The most a process can do
	 * by messing with this is force probes to be dropped in the wrong place
	 * in itself: and if a process wants to perturb tracing of itself there
	 * are many simpler ways, such as overwriting the DOF symbol before the
	 * ELF constructor runs, etc.
	 *
	 * Note: future use of parsed DOF (after DTrace has been running for a
	 * while) may not be safe, since the daemon may be newer than DTrace
	 * and thus have newer parsed DOF. A version comparison will suffice to
	 * check that: for safety we do it here too.
	 */

	assert(pvp->impl != NULL && pvp->impl->provide_probe != NULL);

	if (asprintf(&probepath, "%s/probes/%i/%s/%s/%s/%s", dtp->dt_dofstash_path,
		     pid, pdp->prv[0] == '\0' ? "*" : pdp->prv,
		     pdp->mod[0] == '\0' ? "*" : pdp->mod,
		     pdp->fun[0] == '\0' ? "*" : pdp->fun,
		     pdp->prb[0] == '\0' ? "*" : pdp->prb) < 0)
		goto scan_err;

	switch(glob(probepath, GLOB_NOSORT | GLOB_ERR | GLOB_PERIOD, NULL, &probeglob)) {
	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		/*
		 * Directory missing?  PID not present or has no DOF, which is
		 * fine, though it might lead to a match failure later on.
		 */
		if (errno == ENOENT)
			return 0;

		dt_dprintf("Cannot glob probe components in %s: %s\n", probepath, strerror(errno));
		goto scan_err;
	case GLOB_NOMATCH:
		/* No probes match, which is fine. */
		return 0;
	}

	/* Set dpr and grab the process, if necessary. */
	if (dpr_caller == 0) {
		if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING |
						DTRACE_PROC_SHORTLIVED) < 0) {
			dt_pid_error(dtp, NULL, D_PROC_GRAB,
				     "failed to grab process %d", (int)pid);
			return -1;
		}
		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);
	}

	/* Loop over USDT probes. */
	for (size_t i = 0; i < probeglob.gl_pathc; i++) {
		char *dof_buf = NULL, *p;
		struct stat s;
		char *path;
		size_t dof_buf_size, seen_size = 0, payload_size;
		uint64_t *dof_version;
		char *prv, *mod, *fun, *prb;
		dof_parsed_t *provider, *probe;
		ssize_t nargvlen = 0, xargvlen = 0;
		char *nargv = NULL, *xargv = NULL;
		uint8_t *argmap = NULL;

		/*
		 * Regular files only: in particular, skip . and ..,
		 * which can appear due to GLOB_PERIOD.
		 */
		if ((lstat(probeglob.gl_pathv[i], &s) < 0) ||
		    (!S_ISREG(s.st_mode)))
			continue;

		path = strdup(probeglob.gl_pathv[i]);
		if (path == NULL)
			goto per_mapping_err;

		dof_buf = read_file(path, &dof_buf_size);
		if (dof_buf == NULL || dof_buf_size < sizeof(uint64_t))
			goto per_mapping_err;
		dof_version = (uint64_t *) dof_buf;
		if (*dof_version != DOF_PARSED_VERSION) {
			dt_dprintf("Parsed DOF version incorrect (daemon / running DTrace version skew?) %lli (daemon) versus %i (DTrace)\n",
				   (long long) *dof_version, DOF_PARSED_VERSION);
			goto per_mapping_err;
		}
		p = dof_buf + sizeof(uint64_t);
		dof_buf_size -= sizeof(uint64_t);

		/*
		 * The first two pieces of parsed DOF are always provider and
		 * probe.
		 */
		provider = (dof_parsed_t *) p;
		if (!validate_dof_record(path, provider, DIT_PROVIDER,
					 DIT_PROVIDER_HEADSZ, dof_buf_size,
					 seen_size))
			goto parse_err;

		/*
		 * Ensure that a validly terminated string follows the record
		 * header.
		 */
		prv = provider->provider.name;
		payload_size = provider->size - DIT_PROVIDER_HEADSZ;
		if (memchr(prv, '\0', payload_size) == NULL)
			goto parse_err;

		p += provider->size;
		seen_size += provider->size;

		probe = (dof_parsed_t *) p;
		if (!validate_dof_record(path, probe, DIT_PROBE,
					 DIT_PROBE_HEADSZ, dof_buf_size,
					 seen_size))
			goto parse_err;

		mod = probe->probe.name;
		payload_size = probe->size - DIT_PROBE_HEADSZ;
		if (memchr(mod, '\0', payload_size) == NULL)
			goto parse_err;

		fun = mod + strlen(mod) + 1;
		payload_size -= strlen(mod) + 1;
		if (memchr(fun, '\0', payload_size) == NULL)
			goto parse_err;
		prb = fun + strlen(fun) + 1;
		payload_size -= strlen(fun) + 1;
		if (memchr(prb, '\0', payload_size) == NULL)
			goto parse_err;

		p += probe->size;
		seen_size += probe->size;

		if (probe->probe.nargc > UINT8_MAX ||
		    probe->probe.xargc > UINT8_MAX)
			goto parse_err;

		/*
		 * Assume the order given in dof_parser.h, for simplicity.
		 */
		if (probe->probe.nargc > 0) {
			dof_parsed_t *args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_NATIVE,
						 DIT_ARGS_NATIVE_HEADSZ,
						 dof_buf_size, seen_size))
				goto parse_err;

			nargv = args->nargs.args;
			payload_size = args->size - DIT_ARGS_NATIVE_HEADSZ;
			if (!validate_string_payload(path, nargv, payload_size,
						     probe->probe.nargc))
				goto parse_err;
			nargvlen = payload_size;
			assert(nargvlen >= 0);

			p += args->size;
			seen_size += args->size;
		}
		if (probe->probe.xargc > 0) {
			dof_parsed_t *args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_XLAT,
						 DIT_ARGS_XLAT_HEADSZ,
						 dof_buf_size, seen_size))
				goto parse_err;

			xargv = args->xargs.args;
			payload_size = args->size - DIT_ARGS_XLAT_HEADSZ;
			if (!validate_string_payload(path, xargv, payload_size,
						     probe->probe.xargc))
				goto parse_err;
			xargvlen = payload_size;
			assert(xargvlen >= 0);

			p += args->size;
			seen_size += args->size;
			args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_MAP,
						 DIT_ARGS_MAP_HEADSZ,
						 dof_buf_size, seen_size))
				goto parse_err;

			argmap = args->argmap.argmap;
			payload_size = args->size - DIT_ARGS_MAP_HEADSZ;
			if (!validate_argmap_payload(path, argmap, payload_size,
						     probe->probe.nargc,
						     probe->probe.xargc))
				goto parse_err;

			p += args->size;
			seen_size += args->size;
		}

		/*
		 * Now the parsed DOF for this probe's tracepoints.
		 */
		for (size_t j = 0; j < probe->probe.ntp; j++) {
			dof_parsed_t *tp = (dof_parsed_t *) p;
			pid_probespec_t psp = {0};
			const prmap_t *pmp;

			if (!validate_dof_record(path, tp, DIT_TRACEPOINT,
						 DIT_TRACEPOINT_HEADSZ,
						 dof_buf_size, seen_size))
				goto parse_err;

			p += tp->size;
			seen_size += tp->size;

			/*
			 * Check for process death in the inner loop to handle
			 * the process dying while its DOF is being pulled in.
			 */
			if (Pstate(dpr->dpr_proc) == PS_DEAD)
				continue;

			pmp = Paddr_to_map(dpr->dpr_proc, tp->tracepoint.addr);
			if (!pmp) {
				dt_dprintf("%i: cannot determine 0x%lx's mapping\n",
					   Pgetpid(dpr->dpr_proc), tp->tracepoint.addr);
				continue;
			}

			psp.pps_fn = Pmap_mapfile_name(dpr->dpr_proc, pmp);
			if (psp.pps_fn == NULL) {
				dt_pid_error(dtp, dpr, D_PROC_USDT,
					"Cannot get name of mapping containing "
					"%sprobe %s for pid %d\n",
					tp->tracepoint.is_enabled ? "is-enabled ": "",
					prb, dpr->dpr_pid);
				goto oom;
			}

			psp.pps_type = tp->tracepoint.is_enabled ? DTPPT_IS_ENABLED : DTPPT_USDT;
			psp.pps_prv = prv;
			psp.pps_mod = mod;
			psp.pps_fun = fun;
			psp.pps_prb = prb;
			psp.pps_dev = pmp->pr_dev;
			psp.pps_inum = pmp->pr_inum;
			psp.pps_pid = dpr->dpr_pid;
			psp.pps_off = tp->tracepoint.addr - pmp->pr_file->first_segment->pr_vaddr;
			psp.pps_nameoff = 0;

			if (nargv) {
				psp.pps_nargc = probe->probe.nargc;
				psp.pps_nargvlen = nargvlen;
				psp.pps_nargv = nargv;
			}

			if (xargv) {
				psp.pps_xargc = probe->probe.xargc;
				psp.pps_xargvlen = xargvlen;
				psp.pps_xargv = xargv;
			}

			if (argmap)
				psp.pps_argmap = argmap;

			if (tp->tracepoint.args[0] != 0) {
				psp.pps_sargv = tp->tracepoint.args;

				payload_size = tp->size - DIT_TRACEPOINT_HEADSZ;
				if (memchr(psp.pps_sargv, '\0',
					   payload_size) == NULL)
					goto parse_err;
			}

			dt_dprintf("providing %s:%s:%s:%s for pid %d @ %lx\n",
				   psp.pps_prv, psp.pps_mod, psp.pps_fun,
				   psp.pps_prb, psp.pps_pid, psp.pps_off);
			if (pvp->impl->provide_probe(dtp, &psp) < 0) {
				dt_pid_error(dtp, dpr, D_PROC_USDT,
					"failed to instantiate %sprobe %s for pid %d: %s",
					tp->tracepoint.is_enabled ? "is-enabled ": "",
					psp.pps_prb, psp.pps_pid,
					dtrace_errmsg(dtp, dtrace_errno(dtp)));
				ret = -1;
			}
			free(psp.pps_fn);
		}

		free(path);
		free(dof_buf);
		continue;

	  parse_err:
		dt_dprintf("Parsed DOF corrupt. This should never happen.\n");
	  oom: ;
	  per_mapping_err:
		free(path);
		free(dof_buf);
		globfree(&probeglob);
		if (dpr_caller == 0)
			dt_proc_release_unlock(dtp, pid);
		return -1;
	}

	globfree(&probeglob);
	if (dpr_caller == 0) {
		dt_pid_fix_mod(NULL, pdp, dtp, pid);
		dt_proc_release_unlock(dtp, pid);
	}
	return ret;

scan_err:
	dt_dprintf("Cannot read DOF stash directory %s: %s\n",
		   probepath, strerror(errno));
	return -1;
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
dt_pid_get_pid(const dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
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
		dt_pid_error(dtp, dpr, D_PROC_BADPROV,
			     "'%s' is not a valid provider", pdp->prv);
		return -1;
	}

	errno = 0;
	pid = strtol(last, &end, 10);

	if (errno != 0 || end == last || end[0] != '\0' || pid <= 0) {
		dt_pid_error(dtp, dpr, D_PROC_BADPID,
			     "'%s' does not contain a valid pid", pdp->prv);
		return -1;
	}

	return pid;
}

/*
 * Create pid probes.  Return 0 on success (even if no probes are
 * created, since there might still be USDT probes) and -1 on error.
 */
int
dt_pid_create_pid_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp)
{
	pid_t pid;
	char provname[DTRACE_PROVNAMELEN];
	dt_proc_t *dpr;
	int err;

	/* Exclude pid0 from being specifically requested. */
	if (strcmp(pdp->prv, "pid0") == 0) {
		dt_pid_error(dtp, NULL, D_PROC_BADPID,
			     "pid0 does not contain a valid pid");
		return -1;
	}

	/* Extract the pid. */
	pid = dt_pid_get_pid(pdp, dtp, NULL);
	if (pid <= 0)
		return 0;

	/* Check whether pid$pid matches the probe description. */
	snprintf(provname, sizeof(provname), "pid%d", (int)pid);
	if (gmatch(provname, pdp->prv) == 0)
		return 0;

	/* Grab the process. */
	if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING |
					DTRACE_PROC_SHORTLIVED) < 0) {
		dt_pid_error(dtp, NULL, D_PROC_GRAB,
			     "failed to grab process %d", (int)pid);
		return -1;
	}
	dpr = dt_proc_lookup(dtp, pid);
	assert(dpr != NULL);

	/* Create the pid probes for this process. */
	err = dt_pid_create_pid_probes_proc(pdp, dtp, dpr);
	dt_proc_release_unlock(dtp, pid);

	return err;
}

static int
dt_stapsdt_parse(dtrace_hdl_t *dtp, dt_proc_t *dpr, dtrace_probedesc_t *pdp,
		 const dt_provider_t *pvp, char *path, unsigned long addr_start)
{
	size_t shstrndx, noff, doff, off, n;
	const prmap_t *pmp = NULL;
	char *mapfile = NULL;
	Elf_Scn *scn = NULL;
	Elf *elf = NULL;
	GElf_Shdr shdr;
	GElf_Ehdr ehdr;
	GElf_Nhdr nhdr;
	Elf_Data *data;
	int i, err = 0;
	int fd = -1;
	char *mod;
	char *no_fun = "";

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		dt_pid_error(dtp, dpr, D_PROC_USDT, "Cannot open %s: %s\n",
			     path, strerror(errno));
		return -1;
	}
	mod = strrchr(path, '/');
	if (mod)
		mod++;
	else
		mod = path;

	elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);   // ELF_C_READ ?

	if (elf_kind(elf) != ELF_K_ELF)
		return -1;
	elf_getshdrstrndx(elf, &shstrndx);

	if (gelf_getehdr(elf, &ehdr)) {
		switch (ehdr.e_type) {
		case ET_EXEC:
			/* binary does not require base addr adjustment */
			addr_start = 0;
			break;
		case ET_DYN:
			break;
		default:
			dt_dprintf("unexpected ELF hdr type 0x%x for '%s'\n",
				   ehdr.e_type, path);
			err = -1;
			goto out;
		}
	}

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		char *secname;

		assert(gelf_getshdr(scn, &shdr) != NULL);

		secname = elf_strptr(elf, shstrndx, shdr.sh_name);
		if (strcmp(secname, SEC_STAPSDT_NOTE) == 0 &&
		    shdr.sh_type == SHT_NOTE)
			break;
	}
	/* No ELF notes, just bail. */
	if (scn == NULL)
		goto out;
	data = elf_getdata(scn, 0);
	for (off = 0;
	     (off = gelf_getnote(data, off, &nhdr, &noff, &doff)) > 0;) {
		char prvname[DTRACE_PROVNAMELEN];
		char prbname[DTRACE_NAMELEN];
		pid_probespec_t psp = {0};
		char *prv, *prb;
		const char *fun;
		char *dbuf = (char *)data->d_buf;
		long *addrs = data->d_buf + doff; /* 3 addrs are loc/base/semaphore */
		GElf_Sym sym;

		if (strncmp(dbuf + noff, NAME_STAPSDT_NOTE, nhdr.n_namesz) != 0)
			continue;
		prv = dbuf + doff + (3*sizeof(long));
		/* ensure prv/prb is null-terminated */
		if (strlen(prv) >= nhdr.n_descsz)
			continue;
		strncpy(prvname, prv, sizeof(prvname));
		prb = prv + strlen(prv) + 1;
		if (strlen(prb) >= nhdr.n_descsz)
			continue;
		strncpy(prbname, prb, DTRACE_NAMELEN);
		(void) strhyphenate(prbname);

		if (strncmp(pdp->prv, prvname, strlen(prvname)) != 0)
			continue;
		/* skip unmatched, non-wildcarded probes */
		if (strcmp(pdp->prb, "*") != 0 &&
		    (strlen(pdp->prb) > 0 && strcmp(pdp->prb, prbname) != 0))
			continue;
		if (prb + strlen(prb) + 1 < dbuf + doff + nhdr.n_descsz) {
			char	*p;

			psp.pps_sargv = prb + strlen(prb) + 1;

			for (p = psp.pps_sargv; (p = strchr(p, '@')) != NULL;
			     p++)
				psp.pps_nargc++;
		}

		psp.pps_type = DTPPT_STAPSDT;
		psp.pps_prv = prvname;
		psp.pps_mod = mod;
		psp.pps_prb = prbname;
		if (elf_getphdrnum(elf, &n))
			continue;

		for (i = 0; i < n; i++) {
			GElf_Phdr phdr;

			if (!gelf_getphdr(elf, i, &phdr))
				break;
			if (addrs[0] >= phdr.p_vaddr &&
			    addrs[0] < phdr.p_vaddr + phdr.p_memsz) {
				psp.pps_off = addrs[0] - phdr.p_vaddr + phdr.p_offset;
			}
			if (!addrs[2])
				continue;
			if (addrs[2] >= phdr.p_vaddr &&
			    addrs[2] < phdr.p_vaddr + phdr.p_memsz)
				psp.pps_refcntr_off = addrs[2] - phdr.p_vaddr + phdr.p_offset;
		}

		if (!psp.pps_off)
			continue;
		psp.pps_nameoff = 0;

		if (!pmp)
			pmp = Paddr_to_map(dpr->dpr_proc, addr_start + addrs[0]);
		if (!pmp) {
			dt_dprintf("%i: cannot determine 0x%lx's mapping\n",
				   Pgetpid(dpr->dpr_proc), psp.pps_off);
			continue;
		}
		if (!mapfile)
			mapfile = Pmap_mapfile_name(dpr->dpr_proc, pmp);

		if (!mapfile) {
			dt_pid_error(dtp, dpr, D_PROC_USDT,
				"Cannot get name of mapping containing probe %s for pid %d\n",
				psp.pps_prb, dpr->dpr_pid);
			err = -1;
			break;
		}
		psp.pps_fn = mapfile;
		if (dt_Plookup_by_addr(dtp, dpr->dpr_pid, addr_start + addrs[0],
				       &fun, &sym) == 0)
			psp.pps_fun = (char *)fun;
		else
			psp.pps_fun = no_fun;
		psp.pps_dev = pmp->pr_dev;
		psp.pps_inum = pmp->pr_inum;
		psp.pps_pid = dpr->dpr_pid;
		psp.pps_nameoff = 0;

		if (pvp->impl->provide_probe(dtp, &psp) < 0) {
			dt_pid_error(dtp, dpr, D_PROC_USDT,
				"failed to instantiate probe %s for pid %d: %s",
				psp.pps_prb, psp.pps_pid,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
			err = -1;
		}
		if (err == -1)
			break;

		if (psp.pps_fun != no_fun)
			free(psp.pps_fun);
	}

out:
	free(mapfile);
	elf_end(elf);
	close(fd);
	return err;
}

static void
dt_pid_create_stapsdt_probes_proc(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
				  const dt_provider_t *pvp, dt_proc_t *dpr,
				  const char *proc_map)
{
	char line[1024];
	FILE *fp = NULL;
	pid_t pid;

	assert(dpr != NULL);

	pid = dpr->dpr_pid;
	fp = fopen(proc_map, "r");
	if (!fp)
		return;

	while (fgets(line, sizeof(line) - 1, fp) != NULL) {
		long addr_start, addr_end, file_offset;
		long dev_major, dev_minor;
		unsigned long inode;
		char name[PATH_MAX + 1];
		char path[PATH_MAX + 1];
		char perm[5];
		int ret;

		ret = sscanf(line,
			     "%lx-%lx %4s %lx %lx:%lx %lu %[^\n]",
			     &addr_start, &addr_end, perm, &file_offset,
			     &dev_major, &dev_minor, &inode, name);
		if (ret != 8 || !strchr(perm, 'x') || strchr(name, '[') != NULL)
			continue;

		/* libstapsdt uses an memfd-based library to dynamically create
		 * stapsdt notes for dynamic languages like python; we need
		 * the associated /proc/<pid>/fds/ fd to read these notes.
		 */
		if (strncmp(name, "/memfd:", strlen("/memfd:")) == 0) {
			DIR *d;
			struct dirent *dirent;
			char *deleted;

			deleted = strstr(name, " (deleted)");
			if (deleted)
				*deleted = '\0';
			snprintf(path, sizeof(path), "/proc/%d/fd", pid);
			d = opendir(path);
			if (d == NULL)
				continue;
			while ((dirent = readdir(d)) != NULL) {
				struct stat s;

				snprintf(path, sizeof(path), "/proc/%d/fd/%s",
					 pid, dirent->d_name);
				if (stat(path, &s) != 0 || s.st_ino != inode)
					continue;
				if (dt_stapsdt_parse(dtp, dpr, pdp, pvp, path,
						     addr_start - file_offset) != 0)
					break;
			}
		} else {
			if (dt_stapsdt_parse(dtp, dpr, pdp, pvp, name,
					     addr_start - file_offset) != 0)
				break;
		}
	}
	fclose(fp);
}

static int
dt_pid_create_stapsdt_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp)
{
	const dt_provider_t *pvp;
	dt_proc_t *dpr = NULL;
	const char *pidstr;
	pid_t pid;
	size_t len = strlen(pdp->prv);

	if (len == 0)
		return 0;

	pidstr = &pdp->prv[len];

	while (isdigit(*(pidstr - 1)))
		pidstr--;
	if (strlen(pidstr) == 0)
		return 0;

	pvp = dt_provider_lookup(dtp, "stapsdt");
	assert(pvp != NULL);

	pid = atoll(pidstr);
	if (pid <= 0)
		return 0;
	if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING |
			      DTRACE_PROC_SHORTLIVED) < 0) {
		dt_pid_error(dtp, NULL, D_PROC_GRAB,
			     "failed to grab process %d", (int)pid);
		return 1;
	}
	dpr = dt_proc_lookup(dtp, pid);
	if (dpr) {
		char *path = NULL;

		if (asprintf(&path, "/proc/%s/maps", pidstr) == -1) {
			if (dtp->dt_pcb != NULL)
				longjmp(dtp->dt_pcb->pcb_jmpbuf, EDT_NOMEM);
			else
				path = NULL;
		}

		if (path != NULL) {
			dt_pid_create_stapsdt_probes_proc(pdp, dtp, pvp, dpr,
							  path);
			free(path);
		}

		dt_proc_release_unlock(dtp, pid);
	}

	return 0;
}

int
dt_pid_create_usdt_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp)
{
	glob_t globbuf;
	char *globpat = NULL;
	int err = 0, i, nmatches = 0;

	/* If the function name is "-", we're done. */
	if (pdp->fun[0] == '-' && pdp->fun[1] == '\0')
		return 0;

	/* If it cannot end with a pid, we're done. */
	if (pdp->prv[0] != '\0') {
		char lastchar = pdp->prv[strlen(pdp->prv) - 1];

		if (lastchar != '*' && !isdigit(lastchar))
			return 0;
	}

	/* If it's strictly a pid provider, we're done. */
	if (strncmp(pdp->prv, "pid", 3) == 0 && isdigit(pdp->prv[3])) {
		const char *p = &pdp->prv[4];

		while (isdigit(*p))
			p++;
		if (*p == '\0')
			return 0;
	}

	/* Look for USDT probes. */
	asprintf(&globpat, "%s/probes/*/%s", dtp->dt_dofstash_path, pdp->prv[0] ? pdp->prv : "*");
	nmatches = glob(globpat, 0, NULL, &globbuf) ? 0 : globbuf.gl_pathc;
	for (i = 0; i < nmatches; i++) {
		char *s = globbuf.gl_pathv[i]
			  + strlen(dtp->dt_dofstash_path)
			  + strlen("/probes/");
		pid_t pid;
		dtrace_probedesc_t pdptmp;

		/* Pull out the pid. */
		pid = atoll(s);

		/* Check, since dtprobed takes a while to clean up dead processes. */
		if (!Pexists(pid))
			continue;

		/* Construct the probe descriptor. */
		pdptmp.prv = strchr(s, '/') + 1;
		pdptmp.mod = pdp->mod[0] == '\0' ? "*" : pdp->mod;
		pdptmp.fun = pdp->fun[0] == '\0' ? "*" : pdp->fun;
		pdptmp.prb = pdp->prb[0] == '\0' ? "*" : pdp->prb;

		/* Create USDT probes for this process. */
		if (dt_pid_create_usdt_probes_proc(dtp, pid, NULL, &pdptmp))
			err = 1;
	}
	free(globpat);
	globfree(&globbuf);

	if (err == 0)
		err = dt_pid_create_stapsdt_probes(pdp, dtp);

	/* If no errors, report success. */
	if (err == 0)
		return 0;

	/* If provider description was blank, report success. */
	if (pdp->prv[0] == '\0')
		return 0;

	/* Look to see if the provider description had a pid glob. */
	for (i = strlen(pdp->prv) - 1; i >= 0; i--) {
		/*
		 * If we hit a '*' before a nondigit, we have a pid glob.
		 * So, even though err==0, we declare success.
		 */
		if (pdp->prv[i] == '*')
			return 0;

		/*
		 * If we hit a nondigit before a '*', we do not have a pid glob.
		 * Since a pid was specified explicitly, err==1 means an error.
		 */
		if (!isdigit(pdp->prv[i]))
			return -1;
	}

	/*
	 * If the provider description was exclusively digits,
	 * it was not a legitimate USDT provider description.
	 * So it makes perfect sense not to return any probes.
	 */
	return 0;
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
			pid = dt_pid_get_pid(pdp, dtp, dpr);
			if (pid != dpr->dpr_pid)
				continue;

			pd = *pdp;
			pd.fun = strdup(pd.fun);	/* we may change it */

			if (gmatch(provname, pdp->prv) != 0 &&
			    dt_pid_create_pid_probes_proc(&pd, dtp, dpr) != 0)
				ret = 1;

			/*
			 * If it's not strictly a pid provider, we might match
			 * a USDT provider.
			 */
			if (strcmp(provname, pdp->prv) != 0) {
				if (dt_pid_create_usdt_probes_proc(dtp, -1, dpr, pdp) < 0)
					ret = 1;
				else
					dt_pid_fix_mod(NULL, pdp, dtp, dpr->dpr_pid);
			}

			free((char *)pd.fun);
		}
	}

	/*
	 * XXX systemwide: rescan for new probes here?  We have to do it
	 * at some point, but when?
	 */

	return ret;
}
