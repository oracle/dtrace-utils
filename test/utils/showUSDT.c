/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHT_SUNW_dof
# define SHT_SUNW_dof	0x6ffffff4
#endif

static int	arch;

static void printComments(dof_sec_t *sec, void *data) {
    printf("    Comment = %s\n", (char *)data + sec->dofs_offset);
}

static void printUTSName(dof_sec_t *sec, void *data) {
    struct utsname	*uts = (struct utsname *)((char *)data + sec->dofs_offset);

    printf("    UTS.sysname  = %s\n" \
	   "    UTS.nodename = %s\n" \
	   "    UTS.release  = %s\n" \
	   "    UTS.version  = %s\n" \
	   "    UTS.machine  = %s\n",
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
			 unsigned int *enotab, unsigned char *argtab) {
    int			i;
    unsigned int	*offs;

    printf("      Probe %d: %s:%s:%s:%s\n",
	   id, name, "",
	   strtab + probe->dofpr_func, strtab + probe->dofpr_name);
    printf("        argc %d noffs %d nenoffs %d\n",
	   probe->dofpr_nargc, probe->dofpr_noffs, probe->dofpr_nenoffs);

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

static int readObj(const char *fn) {
    int		fd;
    int		ret = 1;
    Elf		*elf = NULL;
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
	case EM_386:
	case EM_X86_64:
	case EM_SPARC:
	case EM_SPARCV9:
	    arch = ehdr.e_machine;
	    break;
	default:
	    printf("Unsupported ELF machine type %d for object file: %s\n",
		   ehdr.e_machine, fn);
	    goto out;
    }

    printf("Processing %s:\n", fn);

    scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
	if (gelf_getshdr(scn, &shdr) == NULL)
	    goto err;

	if (shdr.sh_type == SHT_SUNW_dof)
	    break;
    }

    if (shdr.sh_type != SHT_SUNW_dof) {
	printf("  No DOF section found!\n");
	goto out;
    }

    if ((data = elf_getdata(scn, NULL)) == NULL) {
	printf("  Failed to read data for SUNW_dof section!\n");
	goto out;
    }

    processDOF(data->d_buf, data->d_size);

    ret = 0;
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
