/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#define	ELF_TARGET_ALL
#include <elf.h>

#include <sys/types.h>

#define	SHN_SUNW_IGNORE	0xff3f

#include <unistd.h>
#include <string.h>
#include <alloca.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <assert.h>
#include <sys/ipc.h>

#include <dt_impl.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_program.h>
#include <dt_string.h>
#include <sys/usdt_note_defs.h>

_dt_printflike_(4,5)
static int
dt_link_error(dtrace_hdl_t *dtp, Elf *elf, int fd, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, format, ap);
	va_end(ap);

	if (elf != NULL)
		elf_end(elf);

	if (fd >= 0)
		close(fd);

	return dt_set_errno(dtp, EDT_COMPILER);
}

typedef struct usdt_note usdt_note_t;
struct usdt_note {
	usdt_note_t	*next;
	char		*data;
	size_t		size;
};

typedef struct usdt_elf {
	dtrace_hdl_t	*dtp;
	Elf_Scn		*note;
	off_t		base;
	off_t		size;
	usdt_note_t	notes;
} usdt_elf_t;

#define STORE_ATTR(n, d, c)	(((n) << 24) | ((d) << 16) | ((c) << 8))

static void
note_store_attr(char **p, const dtrace_attribute_t *ap)
{
	uint32_t	*attr = (uint32_t *)*p;

	*attr = STORE_ATTR(ap->dtat_name, ap->dtat_data, ap->dtat_class);
	*p += sizeof(uint32_t);
}

static int
note_add_probe(dt_idhash_t *dhp, dt_ident_t *idp, void *data)
{
	usdt_elf_t	*usdt = data;
	usdt_note_t	*note;
	dt_node_t	*dnp;
	dt_probe_t	*prp = idp->di_data;
	char		*buf, *p;
	size_t		len, sz;
	int		i;
	char		n[DT_TYPE_NAMELEN];

	len = strlen(prp->pr_name);
	sz = ALIGN(len + 1, 4) + 2 * sizeof(uint8_t) +
	     (prp->nargc + prp->xargc) * DT_TYPE_NAMELEN +
	     prp->xargc * sizeof(uint8_t);
	sz = ALIGN(sz, 4);

	buf = malloc(sz);
	if (buf == NULL)
		return dt_set_errno(usdt->dtp, EDT_NOMEM);
	memset(buf, 0, sz);

	p = buf;
	strcpy(p, prp->pr_name);
	p += len + 1;
	*(uint8_t *)p = prp->nargc;
	p++;
	for (dnp = prp->nargs; dnp != NULL; dnp = dnp->dn_list) {
		strcpy(p, ctf_type_name(dnp->dn_ctfp, dnp->dn_type, n, sizeof(n)));
		p += strlen(p) + 1;
	}
	*(uint8_t *)p = prp->xargc;
	p++;
	for (dnp = prp->xargs, i = 0; dnp != NULL; dnp = dnp->dn_list, i++) {
		strcpy(p, ctf_type_name(dnp->dn_ctfp, dnp->dn_type, n, sizeof(n)));
		p += strlen(p) + 1;
		*p++ = prp->mapping[i];
	}

	sz = p - buf;

	note = dt_zalloc(usdt->dtp, sizeof(usdt_note_t));
	note->data = buf;
	note->size = ALIGN(sz, 4);
	note->next = usdt->notes.next;
	usdt->notes.next = note;

	return 0;
}

static int
note_add_provider(usdt_elf_t *usdt, dt_provider_t *pvp)
{
	usdt_note_t	*note;
	Elf_Data	*dbuf;
	char		*buf, *p;
	size_t		len, sz;
	uint32_t	nprobes = dt_idhash_size(pvp->pv_probes);

#define PROV_NOTE_HEADSZ \
		((3 * sizeof(uint32_t)) +	/* namesz, descsz, type */ \
		 ALIGN(strlen(_USDT_PV_NOTE_NAME) + 1, 4))	/* "prov\0" */

	if (!nprobes)
		return 0;

	/* Ensure the note starts on a 4-byte alignment boundary. */
	usdt->base = ALIGN(usdt->base + usdt->size, 4);
	usdt->size = 0;

	/* Ensure there is enough space in the provider name for the PID. */
	len = strlen(pvp->desc.dtvd_name);
	if (len > DTRACE_PROVNAMELEN - 11)
		return dt_link_error(usdt->dtp, NULL, -1,
				     "USDT provider name may not exceed %d "
				     "characters: %s\n",
				     DTRACE_PROVNAMELEN - 11,
				     pvp->desc.dtvd_name);

	sz = PROV_NOTE_HEADSZ +
	     ALIGN(len + 1, 4) +	/* provider name */
	     6 * sizeof(uint32_t);	/* stability attributes */

	buf = malloc(sz);
	if (buf == NULL)
		return dt_set_errno(usdt->dtp, EDT_NOMEM);
	memset(buf, 0, sz);

	/* Add the note header. */
	dbuf = elf_newdata(usdt->note);
	dbuf->d_align = sizeof(uint32_t);
	dbuf->d_off = usdt->base;
	dbuf->d_buf = buf;
	dbuf->d_size = sz;
	usdt->size = sz;

	p = buf + PROV_NOTE_HEADSZ;
	strcpy(p, pvp->desc.dtvd_name);
	p += ALIGN(len + 1, 4);
	note_store_attr(&p, &pvp->desc.dtvd_attr.dtpa_provider);
	note_store_attr(&p, &pvp->desc.dtvd_attr.dtpa_mod);
	note_store_attr(&p, &pvp->desc.dtvd_attr.dtpa_func);
	note_store_attr(&p, &pvp->desc.dtvd_attr.dtpa_name);
	note_store_attr(&p, &pvp->desc.dtvd_attr.dtpa_args);
	*(uint32_t *)p = nprobes;
	p += sizeof(uint32_t);

	/* Add the probe definitions. */
	dt_idhash_iter(pvp->pv_probes, note_add_probe, usdt);

	/* Add the note fragments. */
	note = usdt->notes.next;
	while (note) {
		usdt_note_t	*next = note->next;

		dbuf = elf_newdata(usdt->note);
		dbuf->d_align = sizeof(char);
		dbuf->d_off = usdt->base + usdt->size;
		dbuf->d_buf = note->data;
		dbuf->d_size = note->size;
		usdt->size += note->size;

		dt_free(usdt->dtp, note);
		note = next;
	}

	/* Construct the note header. */
	*((uint32_t *)&buf[0]) = 5;
	*((uint32_t *)&buf[4]) = usdt->size - PROV_NOTE_HEADSZ;
	*((uint32_t *)&buf[8]) = 1;
	memcpy(&buf[12], _USDT_PV_NOTE_NAME, 4);

	usdt->notes.next = NULL;

	return 0;
}

static int
note_add_version(usdt_elf_t *usdt)
{
	Elf_Data	*dbuf;
	char		*buf, *p;
	size_t		len, sz;

#define DVER_NOTE_HEADSZ \
		((3 * sizeof(uint32_t)) +	/* namesz, descsz, type */ \
		 ALIGN(5, 4))			/* "dver\0" */

	/* Ensure the note starts on a 4-byte alignment boundary. */
	usdt->base = ALIGN(usdt->base + usdt->size, 4);
	usdt->size = 0;

	len = strlen(_dtrace_version);
	sz = DVER_NOTE_HEADSZ + ALIGN(len + 1, 4);

	buf = malloc(sz);
	if (buf == NULL)
		return dt_set_errno(usdt->dtp, EDT_NOMEM);
	memset(buf, 0, sz);

	/* Construct the note header. */
	*((uint32_t *)&buf[0]) = 5;
	*((uint32_t *)&buf[4]) = sz - DVER_NOTE_HEADSZ;
	*((uint32_t *)&buf[8]) = 1;
	memcpy(&buf[12], "dver", 4);

	/* Add the data. */
	p = buf + DVER_NOTE_HEADSZ;
	strcpy(p, _dtrace_version);

	/* Add the note header. */
	dbuf = elf_newdata(usdt->note);
	dbuf->d_align = sizeof(uint32_t);
	dbuf->d_off = usdt->base;
	dbuf->d_buf = buf;
	dbuf->d_size = sz;
	usdt->size = sz;

	usdt->notes.next = NULL;

	return 0;
}

static int
note_add_utsname(usdt_elf_t *usdt)
{
	Elf_Data	*dbuf;
	char		*buf, *p;
	size_t		len, sz;

#define UTSN_NOTE_HEADSZ \
		((3 * sizeof(uint32_t)) +	/* namesz, descsz, type */ \
		 ALIGN(5, 4))			/* "utsn\0" */

	/* Ensure the note starts on a 4-byte alignment boundary. */
	usdt->base = ALIGN(usdt->base + usdt->size, 4);
	usdt->size = 0;

	len = sizeof(struct utsname);
	sz = UTSN_NOTE_HEADSZ + ALIGN(len, 4);

	buf = malloc(sz);
	if (buf == NULL)
		return dt_set_errno(usdt->dtp, EDT_NOMEM);
	memset(buf, 0, sz);

	/* Construct the note header. */
	*((uint32_t *)&buf[0]) = 5;
	*((uint32_t *)&buf[4]) = sz - UTSN_NOTE_HEADSZ;
	*((uint32_t *)&buf[8]) = 1;
	memcpy(&buf[12], "utsn", 4);

	/* Add the data. */
	p = buf + UTSN_NOTE_HEADSZ;
	memcpy(p, &usdt->dtp->dt_uts, sizeof(struct utsname));

	/* Add the note header. */
	dbuf = elf_newdata(usdt->note);
	dbuf->d_align = sizeof(uint32_t);
	dbuf->d_off = usdt->base;
	dbuf->d_buf = buf;
	dbuf->d_size = sz;
	usdt->size = sz;

	usdt->notes.next = NULL;

	return 0;
}

static int
create_elf64(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, int fd, uint_t flags)
{
	usdt_elf_t		*usdt;
	Elf			*elf;
	Elf64_Ehdr		*ehdr;
	Elf64_Shdr		*shdr;
	Elf_Scn			*scn;
	Elf_Data		*dbuf;
	dt_htab_next_t		*it = NULL;
	dt_provider_t		*pvp;
	static const char	SHSTRTAB[] = "\0"
					     ".shstrtab\0"		/* 1 */
#define SECT_SHSTRTAB		1
#define NAMEOFF_SHSTRTAB	1	/* .shstrtab */
					     ".note.GNU-stack\0"	/* 11 */
#define SECT_NOTE_GNUTACK	2
#define NAMEOFF_NOTE_GNUSTACK	11	/* .note.GNU-stack */
					     ".note.usdt\0";		/* 27 */
#define SECT_NOTE_USDT		3
#define NAMEOFF_NOTE_USDT	27	/* .note.usdt */

	if (!(flags & DTRACE_D_PROBES))
		return 0;

	usdt = dt_zalloc(dtp, sizeof(usdt_elf_t));
	if (!usdt) {
		dt_set_errno(dtp, EDT_NOMEM);
		goto fail;
	}

	usdt->dtp = dtp;
	elf = elf_begin(fd, ELF_C_WRITE, NULL);
	if (!elf)
		goto fail;

	ehdr = elf64_newehdr(elf);
        ehdr->e_type = ET_REL;
#if defined(__amd64)
        ehdr->e_machine = EM_X86_64;
#elif defined(__aarch64__)
        ehdr->e_machine = EM_AARCH64;
#endif
        ehdr->e_version = EV_CURRENT;
        ehdr->e_shoff = sizeof(Elf64_Ehdr);
        ehdr->e_ehsize = sizeof(Elf64_Ehdr);
        ehdr->e_phentsize = sizeof(Elf64_Phdr);
        ehdr->e_shentsize = sizeof(Elf64_Shdr);
	ehdr->e_shstrndx = 1;

	/* Create .shstrtab */
	scn = elf_newscn(elf);
	shdr = elf64_getshdr(scn);
	shdr->sh_name = NAMEOFF_SHSTRTAB;
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_addralign = sizeof(char);

	dbuf = elf_newdata(scn);
	dbuf->d_size = sizeof(SHSTRTAB);
	dbuf->d_buf = malloc(dbuf->d_size);
	if (!dbuf->d_buf) {
		dt_set_errno(dtp, EDT_NOMEM);
		goto fail;
	}
	memcpy(dbuf->d_buf, SHSTRTAB, dbuf->d_size);

	/* Create .note.GNU-stack */
	scn = elf_newscn(elf);
	shdr = elf64_getshdr(scn);
	shdr->sh_name = NAMEOFF_NOTE_GNUSTACK;
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_addralign = sizeof(char);

	dbuf = elf_newdata(scn);

	/* Create .note.usdt */
	usdt->note = elf_newscn(elf);
	shdr = elf64_getshdr(usdt->note);
	shdr->sh_name = NAMEOFF_NOTE_USDT;
	shdr->sh_type = SHT_NOTE;
	shdr->sh_addralign = sizeof(char);

	/* Add the provider definitions. */
	while ((pvp = dt_htab_next(dtp->dt_provs, &it)) != NULL) {
		if (note_add_provider(usdt, pvp) == -1)
			goto fail;
	}

	if (!(flags & DTRACE_D_STRIP)) {
		if (note_add_version(usdt) == -1)
			goto fail;
		if (note_add_utsname(usdt) == -1)
			goto fail;
	}

	dt_free(dtp, usdt);

	/* Write the ELF object. */
	elf_update(elf, ELF_C_WRITE);
	elf_end(elf);

	return 0;

fail:
	if (usdt) {
		if (elf)
			elf_end(elf);

		dt_free(dtp, usdt);
	}

	return -1;
}

int
dtrace_program_link(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, uint_t dflags,
    const char *file, int objc, char *const objv[])
{
	char drti[PATH_MAX], symvers[PATH_MAX];
	int fd, i, cur;
	char *cmd;
	int ret = 0, status = 0;

	/*
	 * A NULL program indicates a special use in which we just link
	 * together a bunch of object files specified in objv and then
	 * unlink(2) those object files.
	 */
	if (pgp == NULL) {
		const char *fmt = "%s -o %s -r";
		size_t len;

		len = snprintf(NULL, 0, fmt, dtp->dt_ld_path, file) + 1;

		for (i = 0; i < objc; i++)
			len += strlen(objv[i]) + 1;

		cmd = alloca(len);

		cur = snprintf(cmd, len, fmt, dtp->dt_ld_path, file);

		for (i = 0; i < objc; i++)
			cur += snprintf(cmd + cur, len - cur, " %s", objv[i]);

		if ((status = system(cmd)) == -1)
			return dt_link_error(dtp, NULL, -1,
			    "failed to run %s: %s", dtp->dt_ld_path,
			    strerror(errno));

		if (WIFSIGNALED(status))
			return dt_link_error(dtp, NULL, -1,
			    "failed to link %s: %s failed due to signal %d",
			    file, dtp->dt_ld_path, WTERMSIG(status));

		if (WEXITSTATUS(status) != 0)
			return dt_link_error(dtp, NULL, -1,
			    "failed to link %s: %s exited with status %d\n",
			    file, dtp->dt_ld_path, WEXITSTATUS(status));

		for (i = 0; i < objc; i++)
			if (strcmp(objv[i], file) != 0)
				unlink(objv[i]);

		return 0;
	}

	/*
	 * Create a temporary file and then unlink it if we're going to
	 * combine it with drti.o later.  We can still refer to it in child
	 * processes as /dev/fd/<fd>.
	 */
	if ((fd = open64(file, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1)
		return dt_link_error(dtp, NULL, -1,
		    "failed to open %s: %s", file, strerror(errno));

	/*
	 * If -xlinktype=DOF has been selected, just write out the DOF.
	 * Otherwise proceed to the default of generating and linking ELF.
	 */
	switch (dtp->dt_linktype) {
	case DT_LTYP_DOF:
		return dt_link_error(dtp, NULL, -1,
				     "link type %u (DOF) no longer supported\n",
				     dtp->dt_linktype);

	case DT_LTYP_ELF:
		break; /* fall through to the rest of dtrace_program_link() */

	default:
		return dt_link_error(dtp, NULL, -1, "invalid link type %u\n",
				     dtp->dt_linktype);
	}


	if (!dtp->dt_lazyload)
		unlink(file);

	ret = create_elf64(dtp, pgp, fd, dflags | dtp->dt_dflags);
	if (ret == -1)
		goto done;

	if (status != 0 || lseek(fd, 0, SEEK_SET) != 0)
		return dt_link_error(dtp, NULL, -1,
		    "failed to write %s: %s", file, strerror(errno));

	if (!dtp->dt_lazyload) {
		dt_dirpath_t *libdir = dt_list_next(&dtp->dt_lib_path);
		const char *fmt = "%s%s -o %s -r --version-script=%s /dev/fd/%d %s";
		const char *emu = "";

		/*
		 * FIXME: if 32/64-bit dual dtrace is ever revived, we will need
		 * to identify the distinct libdirs properly.
		 */

		if (dtp->dt_oflags & DTRACE_O_ILP32) {
			snprintf(drti, sizeof (drti), "%s/drti/drti32.o", libdir->dir_path);
#if defined(__i386) || defined(__amd64)
			emu = " -m elf_i386";
#endif
		} else {
			snprintf(drti, sizeof (drti), "%s/drti/drti.o", libdir->dir_path);
		}
		snprintf(symvers, sizeof (symvers), "%s/drti/drti-vers", libdir->dir_path);

		asprintf(&cmd, fmt, dtp->dt_ld_path, emu, file, symvers, fd,
			 drti);
		status = system(cmd);
		free(cmd);

		if (status == -1) {
			ret = dt_link_error(dtp, NULL, -1,
			    "failed to run %s: %s", dtp->dt_ld_path,
			    strerror(errno));
			goto done;
		}

		close(fd);

		if (WIFSIGNALED(status)) {
			ret = dt_link_error(dtp, NULL, -1,
			    "failed to link %s: %s failed due to signal %d",
			    file, dtp->dt_ld_path, WTERMSIG(status));
			goto done;
		}

		if (WEXITSTATUS(status) != 0) {
			ret = dt_link_error(dtp, NULL, -1,
			    "failed to link %s: %s exited with status %d\n",
			    file, dtp->dt_ld_path, WEXITSTATUS(status));
			goto done;
		}
	} else
		close(fd); /* release temporary file */

done:
	return ret;
}
