/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.
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
#include <dof_parser.h>

#include <dt_impl.h>
#include <dt_program.h>
#include <dt_provider.h>
#include <dt_pid.h>
#include <dt_string.h>

/*
 * Information on a PID probe.
 */
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
dt_pid_create_one_probe(struct ps_prochandle *P, dtrace_hdl_t *dtp,
    pid_probespec_t *psp, const GElf_Sym *symp, pid_probetype_t type)
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
#error expect init_disassembler_info() to have 3 or else 4 arguments
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
	dt_pcb_t *pcb = pp->dpp_pcb;
	dt_proc_t *dpr = pp->dpp_dpr;
	pid_probespec_t *psp;
	uint64_t off;
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

	if (!isdash && gmatch("return", pp->dpp_name)) {
		if (dt_pid_create_one_probe(pp->dpp_pr, dtp, psp, symp,
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
		if (dt_pid_create_one_probe(pp->dpp_pr, dtp, psp, symp,
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
		char *end;

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

		psp->pps_nameoff = off;
		psp->pps_off = symp->st_value - pp->dpp_vaddr + off;
		if (dt_pid_create_one_probe(pp->dpp_pr, pp->dpp_dtp,
					psp, symp, DTPPT_OFFSETS) < 0) {
			rc = dt_pid_error(
				dtp, pcb, dpr, D_PROC_CREATEFAIL,
				"failed to create probes at '%s+0x%llx': %s",
				func, (unsigned long long)off,
				dtrace_errmsg(dtp, dtrace_errno(dtp)));
			goto out;
		}

		nmatches++;
	} else if (glob && !isdash) {
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

		for (off = 0; off < symp->st_size; off += disasm(off, &disasm_info)) {
			char offstr[32];

#if defined(__amd64)
			/* Newer kernels do not allow uprobes on "hlt" instructions. */
			if ((unsigned int)disasm_info.buffer[off] == 0xf4)
				continue;
#endif

			snprintf(offstr, sizeof(offstr), "%lx", off);
			if (!gmatch(offstr, pp->dpp_name))
				continue;

			psp->pps_nameoff = off;
			psp->pps_off = symp->st_value - pp->dpp_vaddr + off;
			if (dt_pid_create_one_probe(pp->dpp_pr, pp->dpp_dtp,
						psp, symp, DTPPT_OFFSETS) >= 0)
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

/*
 * Create pid probes for the specified process.
 */
static int
dt_pid_create_pid_probes_proc(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp,
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
 * A quick check that a parsed DOF record read hasn't incurred a buffer overrun
 * and is of the type expected.
 */
static int
validate_dof_record(const char *path, const dof_parsed_t *parsed,
		    dof_parsed_info_t expected, size_t buf_size,
		    size_t seen_size)
{
	if (buf_size < seen_size) {
		dt_dprintf("DOF too small when adding probes (seen %zi bytes)\n",
			   seen_size);
		return 0;
	}

	if (parsed->type != expected) {
		dt_dprintf("%s format invalid: expected %i, got %i\n", path,
			   expected, parsed->type);
		return 0;
	}
	return 1;
}


/*
 * Create underlying probes relating to the probespec passed on input.
 *
 * dpr must be set and locked.  Just set up probes relating to mappings found
 * in this one process.
 *
 * Return 0 on success or -1 on error.  (Failure to create specific underlying
 * probes is not an error.)
 */
static int
dt_pid_create_usdt_probes_proc(dtrace_hdl_t *dtp, dt_proc_t *dpr,
			       dtrace_probedesc_t *pdp, dt_pcb_t *pcb)
{
	const dt_provider_t *pvp;
	int ret = 0;
	char *probepath = NULL;
	glob_t probeglob = {0};

	assert(dpr != NULL && dpr->dpr_proc);
	assert(MUTEX_HELD(&dpr->dpr_lock));

	dt_dprintf("Scanning for usdt probes in %i matching %s:%s:%s\n",
		   dpr->dpr_pid, pdp->mod, pdp->fun, pdp->prb);

	pvp = dt_provider_lookup(dtp, "usdt");
	assert(pvp != NULL);

	if (Pstate(dpr->dpr_proc) == PS_DEAD)
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
		     dpr->dpr_pid, pdp->prv[0] == '\0' ? "*" : pdp->prv,
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

	for (size_t i = 0; i < probeglob.gl_pathc; i++) {
		char *dof_buf = NULL, *p;
		struct stat s;
		char *path;
		size_t dof_buf_size, seen_size = 0;
		uint64_t *dof_version;
		char *prv, *mod, *fun, *prb;
		dof_parsed_t *provider, *probe;
		ssize_t nargvlen = 0, xargvlen = 0;
		char *nargv = NULL, *xargv = NULL;
		int8_t *argmap = NULL;

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
		if (dof_buf == NULL)
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
		if (!validate_dof_record(path, provider, DIT_PROVIDER, dof_buf_size,
					 seen_size))
			goto parse_err;

		prv = provider->provider.name;

		p += provider->size;
		seen_size += provider->size;

		probe = (dof_parsed_t *) p;
		if (!validate_dof_record(path, probe, DIT_PROBE, dof_buf_size,
					 seen_size))
			goto parse_err;

		mod = probe->probe.name;
		fun = mod + strlen(mod) + 1;
		prb = fun + strlen(fun) + 1;

		p += probe->size;
		seen_size += probe->size;

		/*
		 * Assume the order given in dof_parser.h, for simplicity.
		 */
		if (probe->probe.nargc > 0) {
			dof_parsed_t *args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_NATIVE,
						 dof_buf_size, seen_size))
				goto parse_err;

			nargv = args->nargs.args;
			nargvlen = args->size - offsetof(dof_parsed_t, nargs.args);
			assert(nargvlen >= 0);

			p += args->size;
			seen_size += args->size;
		}
		if (probe->probe.xargc > 0) {
			dof_parsed_t *args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_XLAT,
						 dof_buf_size, seen_size))
				goto parse_err;

			xargv = args->xargs.args;
			xargvlen = args->size - offsetof(dof_parsed_t, xargs.args);
			assert(xargvlen >= 0);

			p += args->size;
			seen_size += args->size;
			args = (dof_parsed_t *) p;

			if (!validate_dof_record(path, args, DIT_ARGS_MAP,
						 dof_buf_size, seen_size))
				goto parse_err;

			argmap = args->argmap.argmap;

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
				dt_pid_error(dtp, pcb, dpr, D_PROC_USDT,
					     "Cannot get name of mapping containing "
					     "%sprobe %s for pid %d\n",
					     tp->tracepoint.is_enabled ? "is-enabled ": "",
					     psp.pps_prb, dpr->dpr_pid);
				goto oom;
			}

			psp.pps_type = tp->tracepoint.is_enabled ? DTPPT_IS_ENABLED : DTPPT_OFFSETS;
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

			dt_dprintf("providing %s:%s:%s:%s for pid %d\n", psp.pps_prv,
				   psp.pps_mod, psp.pps_fun, psp.pps_prb, psp.pps_pid);
			if (pvp->impl->provide_probe(dtp, &psp) < 0) {
				dt_pid_error(dtp, pcb, dpr, D_PROC_USDT,
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
		return -1;
	}

	globfree(&probeglob);
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
 * Create pid probes.  Return 0 on success (even if no probes are
 * created, since there might still be USDT probes) and -1 on error.
 */
int
dt_pid_create_pid_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb)
{
	pid_t pid;
	char provname[DTRACE_PROVNAMELEN];
	dt_proc_t *dpr;
	int err;

	assert(pcb != NULL);

	/* Exclude pid0 from being specifically requested. */
	if (strcmp(pdp->prv, "pid0") == 0) {
		dt_pid_error(dtp, pcb, NULL, D_PROC_BADPID,
			     "pid0 does not contain a valid pid");
		return -1;
	}

	/* Extract the pid. */
	pid = dt_pid_get_pid(pdp, dtp, pcb, NULL);
	if (pid <= 0)
		return 0;

	/* Check whether pid$pid matches the probe description. */
	snprintf(provname, sizeof(provname), "pid%d", (int)pid);
	if (gmatch(provname, pdp->prv) == 0)
		return 0;

	/* Grab the process. */
	if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING) < 0) {
		dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
		    "failed to grab process %d", (int)pid);
		return -1;
	}
	dpr = dt_proc_lookup(dtp, pid);
	assert(dpr != NULL);

	/* Create the pid probes for this process. */
	err = dt_pid_create_pid_probes_proc(pdp, dtp, pcb, dpr);
	dt_proc_release_unlock(dtp, pid);

	return err;
}

int
dt_pid_create_usdt_probes(dtrace_probedesc_t *pdp, dtrace_hdl_t *dtp, dt_pcb_t *pcb)
{
	glob_t globbuf;
	char *globpat = NULL;
	int err = 0, i, nmatches = 0;

	assert(pcb != NULL);

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
		dt_proc_t *dpr;
		dtrace_probedesc_t pdptmp;

		/* Pull out the pid. */
		pid = atoll(s);

		/* Check, since dtprobed takes a while to clean up dead processes. */
		if (!Pexists(pid))
			continue;

		/* Grab the process. */
		if (dt_proc_grab_lock(dtp, pid, DTRACE_PROC_WAITING |
				      DTRACE_PROC_SHORTLIVED) < 0) {
			dt_pid_error(dtp, pcb, NULL, D_PROC_GRAB,
			    "failed to grab process %d", (int)pid);
			return -1;
		}
		dpr = dt_proc_lookup(dtp, pid);
		assert(dpr != NULL);

		/* Create USDT probes for this process. */
		pdptmp.prv = strchr(s, '/') + 1;
		pdptmp.mod = pdp->mod[0] == '\0' ? "*" : pdp->mod;
		pdptmp.fun = pdp->fun[0] == '\0' ? "*" : pdp->fun;
		pdptmp.prb = pdp->prb[0] == '\0' ? "*" : pdp->prb;
		if (dt_pid_create_usdt_probes_proc(dtp, dpr, &pdptmp, pcb))
			err = 1;

		dt_pid_fix_mod(NULL, &pdptmp, dtp, dpr->dpr_pid);

		dt_proc_release_unlock(dtp, pid);
	}
	free(globpat);
	globfree(&globbuf);

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
			    dt_pid_create_pid_probes_proc(&pd, dtp, NULL, dpr) != 0)
				ret = 1;

			/*
			 * If it's not strictly a pid provider, we might match
			 * a USDT provider.
			 */
			if (strcmp(provname, pdp->prv) != 0) {
				if (dt_pid_create_usdt_probes_proc(dtp, dpr, pdp, NULL) < 0)
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
