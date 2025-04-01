/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/dtrace.h>

#include <dt_htab.h>

#include <ctype.h>

#ifndef SHT_SUNW_dof
# define SHT_SUNW_dof	0x6ffffff4
#endif

#define ALIGN(v, p2)	 (((v) + ((p2) - 1)) & ~((p2) - 1))

static int	arch;

void *dt_alloc(void *dmy, size_t size) {
    return malloc(size);
}

void *dt_calloc(void *dmy, size_t num, size_t size) {
    return calloc(num, size);
}

void dt_free(void *dmy, void *ptr) {
    free(ptr);
}

static void printComments(dof_sec_t *sec, void *data) {
    printf("    Comment:\n" \
	   "      %s\n", (char *)data + sec->dofs_offset);
}

static void printUTSName(dof_sec_t *sec, void *data) {
    struct utsname	*uts = (struct utsname *)((char *)data + sec->dofs_offset);

    printf("    UTS Name:\n" \
	   "      sysname  = %s\n" \
	   "      nodename = %s\n" \
	   "      release  = %s\n" \
	   "      version  = %s\n" \
	   "      machine  = %s\n",
	   uts->sysname, uts->nodename, uts->release, uts->version,
	   uts->machine);
}

static const char *dof_secnames[] = {
					"None",			/*  0 */
					"Comments",
					"Source",
					"ECB Description",
					"Probe Description",
					"Action Description",
					"DIFO Header",
					"DIF",
					"String Table",
					"Variable Table",
					"Relocation Table",	/* 10 */
					"Type Table",
					"User Relocations",
					"Kernel Relocations",
					"Option Description",
					"Provider",
					"Probes",
					"Probe Arguments",
					"Probe Offsets",
					"Integer Table",
					"UTS Name",		/* 20 */
					"XLator Table",
					"XLator Members",
					"XLator Import",
					"XLator Export",
					"Exported Objects",
					"Is-Enabled Probe Offsets"
				    };

static void printSection(dof_sec_t *sec, void *data) {
    if (sec->dofs_entsize == 0)
	printf("DOF section: %s\n", dof_secnames[sec->dofs_type]);
    else
	printf("DOF section: %s [%lu entries]\n",
	       dof_secnames[sec->dofs_type],
	       (long unsigned int)(sec->dofs_size / sec->dofs_entsize));

    printf("             align %d, flags %04x, %lu bytes @ 0x%llx\n",
	   sec->dofs_align, sec->dofs_flags, (long unsigned int)sec->dofs_size,
	   (long long unsigned int)sec->dofs_offset);

    if (sec->dofs_type == DOF_SECT_COMMENTS)
	printComments(sec, data);
    else if (sec->dofs_type == DOF_SECT_UTSNAME)
	printUTSName(sec, data);
}

static void processECB(dof_hdr_t *dof, int size) {
    int		i;
    dof_sec_t	*sec;

    printf("  ECB-form DOF:\n");

    sec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff];
    for (i = 0; i < dof->dofh_secnum; i++, sec++) {
	if (sec->dofs_type != DOF_SECT_ECBDESC)
	    continue;

	printSection(sec, dof);
    }
}

static void processProbe(int id, dof_probe_t *probe, const char *name,
			 const char *strtab, unsigned int *offtab,
			 unsigned int *enotab, const unsigned char *argtab) {
    int			i;
    const char		*str;
    unsigned int	*offs;
    const unsigned char	*map;

    printf("      Probe %d: %s:%s:%s:%s\n",
	   id, name, "",
	   strtab + probe->dofpr_func, strtab + probe->dofpr_name);
    printf("        nargc %d xargc %d noffs %d nenoffs %d\n",
	   probe->dofpr_nargc, probe->dofpr_xargc,
	   probe->dofpr_noffs, probe->dofpr_nenoffs);

    str = strtab + probe->dofpr_nargv;
    for (i = 0; i < probe->dofpr_nargc; i++) {
	printf("          (native) argv[%d]: %s\n",
	       i, str);
	str += strlen(str) + 1;
    }

    str = strtab + probe->dofpr_xargv;
    map = argtab + probe->dofpr_argidx;
    for (i = 0; i < probe->dofpr_xargc; i++) {
	printf("          (translated) argv[%d] (from native argv[%hhd]): %s\n",
	       i, map[i], str);
	str += strlen(str) + 1;
    }

    offs = &offtab[probe->dofpr_offidx];
    for (i = 0; i < probe->dofpr_noffs; i++)
	printf("          Regular probe at 0x%llx + 0x%x = 0x%llx\n",
	       (long long unsigned int)probe->dofpr_addr, offs[i],
	       (long long unsigned int)probe->dofpr_addr + offs[i]);

    offs = &enotab[probe->dofpr_enoffidx];
    for (i = 0; i < probe->dofpr_nenoffs; i++)
	printf("          Is-Enabled probe at 0x%llx + 0x%x = 0x%llx\n",
	       (long long unsigned int)probe->dofpr_addr, offs[i],
	       (long long unsigned int)probe->dofpr_addr + offs[i]);
}

static void processUSDT(dof_hdr_t *dof, int size) {
    int		i;
    dof_sec_t	*sec;

    printf("  USDT-form DOF:\n");

    sec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff];
    for (i = 0; i < dof->dofh_secnum; i++, sec++) {
	int		j, nprobes;
	dof_provider_t	*prov;
	dof_sec_t	*strsec, *offsec, *prbsec, *argsec, *enosec;
	char		*strtab;
	unsigned int	*offtab;
	unsigned int	*enotab;
	unsigned char	*argtab;

	if (sec->dofs_type != DOF_SECT_PROVIDER)
	    continue;

	prov = (dof_provider_t *)&((char *)dof)[sec->dofs_offset];
	strsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     prov->dofpv_strtab *
						dof->dofh_secsize];
	offsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     prov->dofpv_proffs *
						dof->dofh_secsize];
	prbsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     prov->dofpv_probes *
						dof->dofh_secsize];
	argsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     prov->dofpv_prargs *
						dof->dofh_secsize];
	strtab = &((char *)dof)[strsec->dofs_offset];
	offtab = (unsigned int *)&((char *)dof)[offsec->dofs_offset];
	argtab = (unsigned char *)&((char *)dof)[argsec->dofs_offset];

	if (dof->dofh_ident[DOF_ID_VERSION] != DOF_VERSION_1 &&
	    prov->dofpv_prenoffs != DOF_SECT_NONE) {
	    enosec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
						 prov->dofpv_prenoffs *
						 dof->dofh_secsize];
	    enotab = (unsigned int *)&((char *)dof)[enosec->dofs_offset];
	} else {
	    enosec = NULL;
	    enotab = NULL;
	}

	nprobes = prbsec->dofs_size / prbsec->dofs_entsize;

	printf("    Provider '%s' with %d probes:\n",
	       strtab + prov->dofpv_name, nprobes);

	for (j = 0; j < nprobes; j++) {
	    dof_probe_t	*probe;

	    probe = (dof_probe_t *)&((char *)dof)[prbsec->dofs_offset +
						  j * prbsec->dofs_entsize];

	    processProbe(j, probe, strtab + prov->dofpv_name, strtab, offtab,
			 enotab, argtab);
	}
    }

    /*
     * Output any relocations...
     */
    sec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff];
    for (i = 0; i < dof->dofh_secnum; i++, sec++) {
	int		j, nrels;
	dof_relohdr_t	*dofr;
	dof_sec_t	*strsec, *relsec, *tgtsec;
	char		*strtab;
	dof_relodesc_t	*reltab;

	if (sec->dofs_type != DOF_SECT_URELHDR)
	    continue;

	dofr = (dof_relohdr_t *)&((char *)dof)[sec->dofs_offset];
	strsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     dofr->dofr_strtab *
						dof->dofh_secsize];
	relsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     dofr->dofr_relsec *
						dof->dofh_secsize];
	tgtsec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff +
					     dofr->dofr_tgtsec *
						dof->dofh_secsize];
	strtab = &((char *)dof)[strsec->dofs_offset];
	reltab = (dof_relodesc_t *)&((char *)dof)[relsec->dofs_offset];

	nrels = relsec->dofs_size / relsec->dofs_entsize;

	printf("    Relocations: %d entries:\n", nrels);

	for (j = 0; j < nrels; j++) {
	    dof_relodesc_t	*r = &reltab[j];

	    printf("      Relo %d: '%s', type %s, offset %lx\n",
		   j, strtab + r->dofr_name,
		   r->dofr_type == DOF_RELO_NONE ? "NONE" :
			r->dofr_type == DOF_RELO_SETX ? "SETX" : "Unknown",
		   (uint64_t)r->dofr_offset);
	    printf("        (0x%lx + base)\n",
		   *(uint64_t *)&((char *)dof)[tgtsec->dofs_offset +
					       r->dofr_offset]);
	}
    }

    /*
     * Output remaining interesting sections (if they exist)...
     */
    sec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff];
    for (i = 0; i < dof->dofh_secnum; i++, sec++) {
	switch (sec->dofs_type) {
	    case DOF_SECT_COMMENTS:
		printComments(sec, dof);
		break;
	    case DOF_SECT_UTSNAME:
		printUTSName(sec, dof);
		break;
	    default:
		continue;
	}
    }
}

/*
 * DOF supports 26 different sections, yet many sections never appear as a top
 * level object.  DTrace currently supports two forms of DOF: enablings, and
 * providers.
 *
 * Enablings
 * ---------
 *  These are used to register probe enablings (ECB Descriptions) with DTrace.
 *  The userspace consumer passes the DOF in a ENABLE ioctl request.
 *
 *  ECBDesc -+-+-> ProbeDesc ------> Strings
 *           | |
 *           | +-> DIFO -+-+-+-> DIF
 *           |           | | |
 *           |           | | +-> Integers
 *           |           | |
 *           |           | +---> Strings
 *           |           |
 *           |           +-----> Variables
 *           |
 *           +---> Action -+-> DIFO
 *                         |
 *                         +-> Strings
 *
 * Providers
 * ---------
 *  These are used to register providers for userspace SDT probes.  The DOF is
 *  passed to DTrace using the ADDDOF helper ioctl.
 *
 *  Provider -+-+-+-+-> Strings
 *            | | | |
 *            | | | +-> Probe
 *            | | |
 *            | | +---> Arguments
 *            | |
 *            | +-----> Offsets
 *            |
 *            +-------> Is-Enabled Offsets
 */
#define DOF_FORM_UNKNOWN	0
#define DOF_FORM_ECB		1
#define DOF_FORM_USDT		2

static int processDOF(dof_hdr_t *dof, size_t size) {
    int		i, form;
    dof_sec_t	*sec;

    /*
     * We first need to determine what DOF form we're dealing with.  For now,
     * we use a very basic logic:
     *   - if the DOF contains ECB Description sections, it's an enabling;
     *   - otherwise, if it contains Provider sections, it's USDT;
     *   - otherwise, it is an (as yet) unsupported form.
     */
    form = DOF_FORM_UNKNOWN;
    sec = (dof_sec_t *)&((char *)dof)[dof->dofh_secoff];
    for (i = 0; i < dof->dofh_secnum; i++, sec++) {
	if (sec->dofs_type == DOF_SECT_ECBDESC) {
	    form = DOF_FORM_ECB;
	    break;
	} else if (sec->dofs_type == DOF_SECT_PROVIDER) {
	    form = DOF_FORM_USDT;
	    continue;
	}
    }

    if (form == DOF_FORM_ECB) {
	processECB(dof, size);
    } else if (form == DOF_FORM_USDT) {
	processUSDT(dof, size);
    } else {
	printf("  Unsupported DOF form!\n");
	return 1;
    }

    return 0;
}

typedef struct dt_probe		dt_probe_t;
typedef struct dt_provider	dt_provider_t;

struct dt_probe {
    dt_hentry_t		he;
    dt_provider_t	*prov;
    const char		*prv;
    const char		*fun;
    const char		*prb;
};

struct dt_provider {
    dt_hentry_t		he;
    const char		*name;
    uint32_t		pattr;
    uint32_t		mattr;
    uint32_t		fattr;
    uint32_t		nattr;
    uint32_t		aattr;
    uint32_t		probec;
    dt_htab_t		*pmap;
    dt_probe_t		*probes;
};

dt_htab_t	*prvmap;
dt_htab_t	*prbmap;

extern uint32_t str2hval(const char *, uint32_t);

static uint32_t prv_hval(const dt_provider_t *pvp) {
	return str2hval(pvp->name, 0);
}

static int prv_cmp(const dt_provider_t *p, const dt_provider_t *q) {
	return strcmp(p->name, q->name);
}

DEFINE_HE_STD_LINK_FUNCS(prv, dt_provider_t, he)
DEFINE_HTAB_STD_OPS(prv)

static uint32_t prb_hval(const dt_probe_t *prp) {
	uint32_t	hval;

	hval = str2hval(prp->prv, 0);
	hval = str2hval(prp->fun, hval);

	return str2hval(prp->prb, hval);
}

static int prb_cmp(const dt_probe_t *p, const dt_probe_t *q) {
	int	rc;

	if (p->fun != NULL) {
		if (q->fun == NULL)
			return 1;
		else {
			rc = strcmp(p->fun, q->fun);
			if (rc != 0)
				return rc;
		}
	} else if (q->fun == NULL)
		return -1;

	return strcmp(p->prb, q->prb);
}

DEFINE_HE_STD_LINK_FUNCS(prb, dt_probe_t, he)
DEFINE_HTAB_STD_OPS(prb)

static int processNote(const char *name, int type, const char *data, int size,
		       Elf_Data *rodata) {
    int			i;
    dt_probe_t		prbt, *prp;
    dt_provider_t	prvt, *prov;

    printf("    Note %s, type %d:\n", name, type);
    if (strcmp(name, "usdt") == 0) {
	const char	*p = data;
	uint64_t	off, fno;
	const char	*prv, *fun, *prb;

	off = *(uint64_t *)p;
	p += 8;
	fno = *(uint64_t *)p;
	p += 8;

	if (fno < rodata->d_off)
	    return -1;
	fno -= rodata->d_off;
	if (fno >= rodata->d_size)
	    return -1;

	fun = &((char *)rodata->d_buf)[fno];

	printf("      Offset %#lx\n", off);
	printf("      Function Offset %#lx\n", fno);
	prv = p;
	p += strlen(p) + 1;
	prb = p;
	p += strlen(p) + 1;
	printf("      Probe %s::%s:%s%s\n", prv, fun, prb, type == 2 ? " (is-enabled)" : "");
	printf("      ");

	prvt.name = prv;
	prov = dt_htab_lookup(prvmap, &prvt);
	if (prov == NULL) {
	    prov = malloc(sizeof(dt_provider_t));
	    memset(prov, 0, sizeof(dt_provider_t));
	    prov->name = prv;
	    dt_htab_insert(prvmap, prov);
	    prov->pmap = dt_htab_create(&prb_htab_ops);
	}

	prbt.prv = prv;
	prbt.fun = fun;
	prbt.prb = prb;

	prp = dt_htab_lookup(prbmap, &prbt);
	if (prp == NULL) {
	    prp = malloc(sizeof(dt_probe_t));
	    memset(prp, 0, sizeof(dt_probe_t));
	    prp->prv = prov->name;
	    prp->fun = fun;
	    prp->prb = prb;
	    dt_htab_insert(prbmap, prp);
	}

	for (i = p - data; i < size; i++)
	    printf("%c", isprint(data[i]) ? data[i] : '.');
	printf("\n");
    } else if (strcmp(name, "prov") == 0) {
	const char	*p = data;
	const uint32_t	*attr;
	dt_provider_t	*prov;

	prvt.name = p;
	prov = dt_htab_lookup(prvmap, &prvt);
	if (prov == NULL) {
	    prov = malloc(sizeof(dt_provider_t));
	    memset(prov, 0, sizeof(dt_provider_t));
	    prov->name = p;
	    dt_htab_insert(prvmap, prov);
	    prov->pmap = dt_htab_create(&prb_htab_ops);
	}

	p += ALIGN(strlen(p) + 1, 4);
	attr = (uint32_t *)ALIGN((uintptr_t)p, 4);
	prov->pattr = *attr++;
	prov->mattr = *attr++;
	prov->fattr = *attr++;
	prov->nattr = *attr++;
	prov->aattr = *attr++;
	prov->probec = *attr++;
	printf("      Provider '%s' with %d probe%s:\n",
	       prov->name, prov->probec, prov->probec == 1 ? "" : "s");

	p = (char *)attr;
	for (i = 0; i < prov->probec; i++) {
	    int	j, argc;

	    p = (const char *)ALIGN((uintptr_t)p, 4);
	    prbt.prv = prov->name;
	    prbt.fun = NULL;
	    prbt.prb = p;

	    prp = dt_htab_lookup(prov->pmap, &prbt);
	    if (prp == NULL) {
		prp = malloc(sizeof(dt_probe_t));
		memset(prp, 0, sizeof(dt_probe_t));
		prp->prv = prov->name;
		prp->fun = NULL;
		prp->prb = p;
		dt_htab_insert(prov->pmap, prp);
	    }

	    printf("      Probe %d: %s:::%s\n", i, prp->prv, prp->prb);

	    p += strlen(p) + 1;
	    argc = *(uint8_t *)p++;

	    for (j = 0; j < argc; j++) {
		printf("          (native) argv[%d]: %s\n", j, p);
		p += strlen(p) + 1;
	    }

	    argc = *(uint8_t *)p++;

	    for (j = 0; j < argc; j++) {
		const char	*arg;
		uint8_t		m;

		arg = p;
		p += strlen(arg) + 1;
		m = *p++;
		printf("          (translated) argv[%d] (from native argv[%hhd]): %s\n",
		       j, m, arg);
	    }
	}
    } else if (strcmp(name, "dver") == 0) {
	const char	*p = data;

	printf("      %s:\n", p);
    } else if (strcmp(name, "utsn") == 0) {
	struct utsname	*uts = (struct utsname *)(char *)data;

	printf("      UTS Name:\n" \
	       "        sysname  = %s\n" \
	       "        nodename = %s\n" \
	       "        release  = %s\n" \
	       "        version  = %s\n" \
	       "        machine  = %s\n",
	       uts->sysname, uts->nodename, uts->release, uts->version,
	       uts->machine);
    }

    return 0;
}

static int processNotes(char *data, size_t size, Elf_Data *rodata) {
    size_t		idx = 0;
    int			ret = 0;
    dt_htab_next_t	*hit;
    dt_provider_t	*pvp;
    dt_probe_t		*prp;

    prvmap = dt_htab_create(&prv_htab_ops);
    prbmap = dt_htab_create(&prb_htab_ops);

    for (idx = 0; idx < size; ) {
	int	nsz, dsz, type;

	nsz = *((int *)&data[idx]);
	dsz = *((int *)&data[idx + 4]);
	type = *((int *)&data[idx + 8]);

	ret = processNote(&data[idx + 12], type,
			  &data[idx + 12 + ALIGN(nsz, 4)], dsz, rodata);
	if (ret != 0)
	    break;

	idx += 12 + ALIGN(nsz, 4) + ALIGN(dsz, 4);
    }

    printf("----------\n");
    /* List all providers. */
    hit = NULL;
    while ((pvp = dt_htab_next(prvmap, &hit)) != NULL) {
	printf("      Provider '%s' with %d probe%s:\n",
	       pvp->name, pvp->probec, pvp->probec == 1 ? "" : "s");
    }

    /* List all probes (to be added/validated) to providers. */
    hit = NULL;
    while ((prp = dt_htab_next(prbmap, &hit)) != NULL) {
	printf("      Probe %s::%s:%s\n", prp->prv, prp->fun, prp->prb);
    }

    return ret;
}

static int readObj(const char *fn) {
    int		fd, i;
    int		ret = 1;
    Elf		*elf = NULL;
    Elf_Data	*strtab = NULL;
    Elf_Data	*rodata = NULL;
    char 	*sname;
    Elf_Data	*data;
    Elf_Scn	*scn;
    GElf_Ehdr	ehdr;
    GElf_Shdr	shdr;

    if ((fd = open64(fn, O_RDONLY)) == -1) {
	printf("Failed to open %s: %s\n", fn, strerror(errno));
	return 1;
    }
    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
	printf("Failed to open ELF in %s: %s\n", fn, elf_errmsg(elf_errno()));
	goto out_noelf;
    }
    switch (elf_kind(elf)) {
	case ELF_K_ELF:
	    break;
	case ELF_K_AR:
	    printf("Archives are not supported: %s\n", fn);
	    goto out;
	default:
	    printf("Invalid file type: %s\n", fn);
	    goto out;
    }
    if (gelf_getehdr(elf, &ehdr) == NULL) {
	printf("File corrupted: %s\n", fn);
	goto out;
    }
    switch (ehdr.e_machine) {
	case EM_X86_64:
	case EM_AARCH64:
	    arch = ehdr.e_machine;
	    break;
	default:
	    printf("Unsupported ELF machine type %d for object file: %s\n",
		   ehdr.e_machine, fn);
	    goto out;
    }

    printf("Processing %s:\n", fn);

    /* Get the .shstrtab data. */
    for (i = 1, scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; i++) {
	if (i == ehdr.e_shstrndx)
		break;
    }
    if (scn == NULL ||
	gelf_getshdr(scn, &shdr) == NULL ||
	shdr.sh_type != SHT_STRTAB) {
	printf("  Section .shstrtab not found!\n");
	goto out;
    }
    if ((strtab = elf_getdata(scn, NULL)) == NULL) {
	printf("  Failed to read data for .shstrtab section!\n");
	goto out;
    }

    /* Get the .rodata data. */
    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	if (gelf_getshdr(scn, &shdr) == NULL)
	    goto err;
	if (shdr.sh_type != SHT_PROGBITS)
	    continue;
	if (shdr.sh_name >= strtab->d_size) {
	    printf("  Invalid section name!\n");
	    goto out;
	}
	sname = &((char *)strtab->d_buf)[shdr.sh_name];
	if (strcmp(sname, ".rodata") == 0)
	    break;
    }

    if (scn == NULL) {
	printf("  No .rodata section found!\n");
	goto out;
    }

    if ((rodata = elf_getdata(scn, NULL)) == NULL) {
	printf("  Failed to read data for .rodata section!\n");
	goto out;
    }

    /* (Ab)use the offset to store the load base for the data. */
    rodata->d_off = shdr.sh_addr;

    /* Process note sections. */
    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	if (gelf_getshdr(scn, &shdr) == NULL)
	    goto err;

	switch (shdr.sh_type) {
	    case SHT_NOTE:
		if (shdr.sh_name >= strtab->d_size) {
		    printf("  Invalid section name!\n");
		    goto out;
		}

		sname = &((char *)strtab->d_buf)[shdr.sh_name];
		if (strcmp(sname, ".note.usdt") != 0)
			continue;

		if ((data = elf_getdata(scn, NULL)) == NULL) {
		    printf("  Failed to read data for .note.usdt section!\n");
		    goto out;
		}

		ret = processNotes(data->d_buf, data->d_size, rodata);
		if (ret != 0)
			goto out;

		break;
	    case SHT_SUNW_dof:
		if ((data = elf_getdata(scn, NULL)) == NULL) {
		    printf("  Failed to read data for .note.usdt section!\n");
		    goto out;
		}

		ret = processDOF(data->d_buf, data->d_size);
		if (ret != 0)
			goto out;

		break;
	    default:
		continue;
	}
    }

    goto out;
err:
    printf("  An error was encountered while processing %s\n", fn);
out:
    elf_end(elf);
out_noelf:
    close(fd);

    return ret;
}

int main(int argc, char *argv[]) {
    int		i;

    if (elf_version(EV_CURRENT) == EV_NONE) {
	fprintf(stderr, "Invalid current ELF version.\n");
	exit(1);
    }
    if (argc < 2) {
	fprintf(stderr, "No object files specified.\n");
	exit(1);
    }

    for (i = 1; i < argc; i++)
	readObj(argv[i]);
}
