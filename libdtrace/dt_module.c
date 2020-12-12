/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <elf.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <port.h>

#include <zlib.h>

#include <dt_strtab.h>
#include <dt_kernel_module.h>
#include <dt_module.h>
#include <dt_impl.h>
#include <dt_string.h>

#define KSYM_NAME_MAX 128		    /* from kernel/scripts/kallsyms.c */
#define GZCHUNKSIZE (1024*512)		    /* gzip uncompression chunk size */

static void
dt_module_unload(dtrace_hdl_t *dtp, dt_module_t *dmp);

static void
dt_module_shuffle_to_start(dtrace_hdl_t *dtp, const char *name);

static void
dt_kern_module_find_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp);

/*
 * Symbol table management for userspace modules, via ELF parsing.
 */

static void
dt_module_symhash_insert(dt_module_t *dmp, const char *name, uint_t id)
{
	dt_modsym_t *dsp = &dmp->dm_symchains[dmp->dm_symfree];
	uint_t h;

	assert(dmp->dm_symfree < dmp->dm_nsymelems + 1);

	dsp->dms_symid = id;
	h = dt_strtab_hash(name, NULL) % dmp->dm_nsymbuckets;
	dsp->dms_next = dmp->dm_symbuckets[h];
	dmp->dm_symbuckets[h] = dmp->dm_symfree++;
}

static GElf_Sym *
dt_module_symgelf32(const Elf32_Sym *src, GElf_Sym *dst)
{
	if (dst != NULL) {
		dst->st_name = src->st_name;
		dst->st_info = src->st_info;
		dst->st_other = src->st_other;
		dst->st_shndx = src->st_shndx;
		dst->st_value = src->st_value;
		dst->st_size = src->st_size;
	}

	return dst;
}

static GElf_Sym *
dt_module_symgelf64(const Elf64_Sym *src, GElf_Sym *dst)
{
	if (dst != NULL)
		memcpy(dst, src, sizeof(GElf_Sym));

	return dst;
}

#ifdef BITS
#error BITS is already defined: look to your header files!
#endif

#define BITS 32
#include <dt_symbol_modops.h>
#undef BITS
#define BITS 64
#include <dt_symbol_modops.h>
#undef BITS

dt_module_t *
dt_module_create(dtrace_hdl_t *dtp, const char *name)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_modbuckets;
	dt_module_t *dmp;

	for (dmp = dtp->dt_mods[h]; dmp != NULL; dmp = dmp->dm_next)
		if (strcmp(dmp->dm_name, name) == 0)
			return dmp;

	if ((dmp = malloc(sizeof(dt_module_t))) == NULL)
		return NULL; /* caller must handle allocation failure */

	memset(dmp, 0, sizeof(dt_module_t));
	strlcpy(dmp->dm_name, name, sizeof(dmp->dm_name));
	dt_list_append(&dtp->dt_modlist, dmp);
	dmp->dm_next = dtp->dt_mods[h];
	dtp->dt_mods[h] = dmp;
	dtp->dt_nmods++;

	if (dtp->dt_conf.dtc_ctfmodel == CTF_MODEL_LP64)
		dmp->dm_ops = &dt_modops_64;
	else
		dmp->dm_ops = &dt_modops_32;

	return dmp;
}

dt_module_t *
dt_module_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_modbuckets;
	dt_module_t *dmp;

	/* 'genunix' is an alias for 'vmlinux'. */

	if (strcmp(name, "genunix") == 0) {
		name = "vmlinux";
	}

	for (dmp = dtp->dt_mods[h]; dmp != NULL; dmp = dmp->dm_next)
		if (strcmp(dmp->dm_name, name) == 0)
			return dmp;

	return NULL;
}

/*ARGSUSED*/
dt_module_t *
dt_module_lookup_by_ctf(dtrace_hdl_t *dtp, ctf_file_t *ctfp)
{
	return ctfp ? ctf_getspecific(ctfp) : NULL;
}

static int
dt_module_init_elf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	int fd, err, bits;
	size_t shstrs;

	if ((dmp->dm_flags & DT_DM_BUILTIN) &&
	    (dtp->dt_ctf_elf != NULL)) {
		dmp->dm_elf = dtp->dt_ctf_elf;
		dmp->dm_ops = dtp->dt_ctf_ops;
		dtp->dt_ctf_elf_ref++;
		return 0;
	}

	if (!dmp->dm_file) {
		dt_dprintf("failed to open ELF file for module %s: "
		    "no file name known\n", dmp->dm_name);
		return dt_set_errno(dtp, EDT_NOTLOADED);
	}

	if ((fd = open(dmp->dm_file, O_RDONLY)) == -1) {
		dt_dprintf("failed to open module %s at %s: %s\n",
		    dmp->dm_name, dmp->dm_file, strerror(errno));
		return dt_set_errno(dtp, EDT_OBJIO);
	}

	/*
	 * Don't hold the fd open forever. (ELF_C_READ followed by
	 * elf_cntl(..., ELF_C_FDREAD) triggers assertion failures in elfutils
	 * at gelf_getshdr() time: ELF_C_READ_MMAP works around this.)
	 */

	dmp->dm_elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
	err = elf_cntl(dmp->dm_elf, ELF_C_FDREAD);
	close(fd);

	if (dmp->dm_elf == NULL || err == -1 ||
	    elf_getshdrstrndx(dmp->dm_elf, &shstrs) == -1) {
		dt_dprintf("failed to load %s: %s\n", dmp->dm_file,
		    elf_errmsg(elf_errno()));
		dt_module_destroy(dtp, dmp);
		return dt_set_errno(dtp, EDT_OBJIO);
	}

	switch (gelf_getclass(dmp->dm_elf)) {
	case ELFCLASS32:
		dmp->dm_ops = &dt_modops_32;
		bits = 32;
		break;
	case ELFCLASS64:
		dmp->dm_ops = &dt_modops_64;
		bits = 64;
		break;
	default:
		dt_dprintf("failed to load %s: unknown ELF class\n",
		    dmp->dm_file);
		dt_module_destroy(dtp, dmp);
		return dt_set_errno(dtp, EDT_ELFCLASS);
	}

	dt_dprintf("opened %d-bit module %s (%s)\n", bits, dmp->dm_name,
	    dmp->dm_file);

	if (dmp->dm_flags & DT_DM_BUILTIN) {
		dtp->dt_ctf_elf = dmp->dm_elf;
		dtp->dt_ctf_ops = dmp->dm_ops;
		dtp->dt_ctf_elf_ref++;
	}

	return 0;
}

static int
dt_module_load_sect(dtrace_hdl_t *dtp, dt_module_t *dmp, ctf_sect_t *ctsp)
{
	const char *s;
	size_t shstrs;
	GElf_Shdr sh;
	Elf_Data *dp;
	Elf_Scn *sp;

	if (elf_getshdrstrndx(dmp->dm_elf, &shstrs) == -1)
		return dt_set_errno(dtp, EDT_NOTLOADED);

	for (sp = NULL; (sp = elf_nextscn(dmp->dm_elf, sp)) != NULL; ) {
		if (gelf_getshdr(sp, &sh) == NULL || sh.sh_type == SHT_NULL ||
		    (s = elf_strptr(dmp->dm_elf, shstrs, sh.sh_name)) == NULL)
			continue; /* skip any malformed sections */

#ifdef HAVE_LIBCTF
		if (sh.sh_entsize == ctsp->cts_entsize &&
		    strcmp(s, ctsp->cts_name) == 0)
			break; /* section matches specification */
#else
		if (sh.sh_type == ctsp->cts_type &&
		    sh.sh_entsize == ctsp->cts_entsize &&
		    strcmp(s, ctsp->cts_name) == 0)
			break; /* section matches specification */
#endif
	}

	/*
	 * If the section isn't found, return success but leave cts_data set
	 * to NULL and cts_size set to zero for our caller.
	 */
	if (sp == NULL || (dp = elf_getdata(sp, NULL)) == NULL)
		return 0;

	ctsp->cts_data = dp->d_buf;
	ctsp->cts_size = dp->d_size;

	dt_dprintf("loaded %s [%s] (%lu bytes)\n",
	    dmp->dm_name, ctsp->cts_name, (ulong_t)ctsp->cts_size);

	return 0;
}

/*
 * Only used for linked-in modules.  Archived modules are uncompressed
 * automatically.
 */
static void *dt_ctf_uncompress(dt_module_t *dmp, ctf_sect_t *ctsp)
{
	z_stream s;
	int ret;
	unsigned char out[GZCHUNKSIZE];
	char *output = NULL;
	size_t out_size = 0;

	s.opaque = Z_NULL;
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.avail_in = ctsp->cts_size;
	s.next_in = (void *)ctsp->cts_data;
	s.next_out = out;
	s.avail_out = GZCHUNKSIZE;

	switch (inflateInit2(&s, 15 + 32)) {
	case Z_OK: break;
	case Z_MEM_ERROR: goto oom;
	default: goto zerr;
	}

	do {
		char *new_output;
		ret = inflate(&s, Z_NO_FLUSH);
		switch (ret) {
		case Z_STREAM_END:
			break;
		case Z_BUF_ERROR:
		case Z_OK:
			if (s.avail_out == GZCHUNKSIZE) {
				s.msg = "no output possible after inflate round";
				goto zerr;
			}
			break;
		case Z_DATA_ERROR:
			inflateEnd(&s);
			goto uncompressed;
		case Z_MEM_ERROR:
			goto oom;
		default:
			goto zerr;
		}

		new_output = realloc(output, out_size +
		    (GZCHUNKSIZE - s.avail_out));

		if (new_output == NULL)
			goto oom;
		output = new_output;

		memcpy(output + out_size, out, (GZCHUNKSIZE - s.avail_out));
		out_size += (GZCHUNKSIZE - s.avail_out);
		s.next_out = out;
		s.avail_out = GZCHUNKSIZE;

	} while ((ret != Z_STREAM_END) && (ret != Z_BUF_ERROR));

	inflateEnd(&s);
	ctsp->cts_size = out_size;
	ctsp->cts_data = output;

	return output;

 uncompressed:
	free(output);
	return NULL;

 zerr:
	inflateEnd(&s);
	free(output);
	dt_dprintf("CTF decompression error in module %s: %s\n", dmp->dm_name,
	    s.msg);
	ctsp->cts_data = NULL;
	ctsp->cts_size = 0;
	return NULL;

 oom:
	free(output);
	dt_dprintf("Out of memory decompressing CTF section for module %s.\n",
	    dmp->dm_name);
	ctsp->cts_data = NULL;
	ctsp->cts_size = 0;
	return NULL;
}

static int
dt_module_load(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	if (dmp->dm_flags & DT_DM_LOADED)
		return 0; /* module is already loaded */

	/*
	 * First find out where the module is, and preliminarily load its CTF.
	 * If this fails, we don't care: the problem will be detected in
	 * dt_module_init_elf().
	 */
	dt_kern_module_find_ctf(dtp, dmp);

	/*
	 * Modules not found in the CTF archive, including non-kernel modules,
	 * must pull in the symbol and string tables (in addition to, perhaps,
	 * the CTF section.)
	 */

	if (!(dmp->dm_flags & DT_DM_CTF_ARCHIVED)) {
		if ((dmp->dm_elf == NULL) && (dt_module_init_elf(dtp, dmp) != 0))
			return -1; /* dt_errno is set for us */

		if (dmp->dm_flags & DT_DM_SHARED) {
			dmp->dm_ctdata_name = malloc(strlen(".ctf.shared_ctf") +
			    strlen(dmp->dm_name) + 1);

			if (dmp->dm_ctdata_name == NULL)
				goto oom;

			strcpy(dmp->dm_ctdata_name, ".ctf.shared_ctf");
		} else if (dmp->dm_flags & DT_DM_BUILTIN) {
			dmp->dm_ctdata_name = malloc(strlen(".ctf.") +
			    strlen(dmp->dm_name) + 1);

			if (dmp->dm_ctdata_name == NULL)
				goto oom;

			strcpy(dmp->dm_ctdata_name, ".ctf.");
			strcat(dmp->dm_ctdata_name, dmp->dm_name);
			dmp->dm_ctdata.cts_name = dmp->dm_ctdata_name;
		} else {
			dmp->dm_ctdata_name = strdup(".ctf");
			dmp->dm_ctdata.cts_name = dmp->dm_ctdata_name;

			if (dmp->dm_ctdata_name == NULL)
				goto oom;
		}

#ifndef HAVE_LIBCTF
		dmp->dm_ctdata.cts_type = SHT_PROGBITS;
		dmp->dm_ctdata.cts_flags = 0;
		dmp->dm_ctdata.cts_offset = 0;
#endif
		dmp->dm_ctdata.cts_data = NULL;
		dmp->dm_ctdata.cts_size = 0;
		dmp->dm_ctdata.cts_entsize = 0;

		/*
		 * Attempt to load and uncompress the module's CTF section.
		 * Note that modules might not contain CTF data: this will
		 * result in a successful load_sect but data of size zero (or,
		 * alas, 1, thanks to a workaround for a bug in objcopy in
		 * binutils 2.20).  We will then fail if dt_module_getctf() is
		 * called, as shown below.
		 */

		if (dt_module_load_sect(dtp, dmp, &dmp->dm_ctdata) == -1) {
			dt_module_unload(dtp, dmp);
			return -1; /* dt_errno is set for us */
		}

		/*
		 * The CTF section is often gzip-compressed.  Uncompress it.
		 */
		if (dmp->dm_ctdata.cts_size > 1)
			dmp->dm_ctdata_data = dt_ctf_uncompress(dmp,
			    &dmp->dm_ctdata);
	}

	/*
	 * Nothing more to do for loaded kernel modules: we already have their
	 * symbols loaded into the dm_kernsyms.
	 */

	if ((dmp->dm_flags & DT_DM_KERNEL) &&
	    (!(dmp->dm_flags & DT_DM_KERN_UNLOADED))) {
		dmp->dm_flags |= DT_DM_LOADED;
		return 0;
	}

	dmp->dm_symtab.cts_name = ".symtab";
#ifndef HAVE_LIBCTF
	dmp->dm_symtab.cts_type = SHT_SYMTAB;
	dmp->dm_symtab.cts_flags = 0;
	dmp->dm_symtab.cts_offset = 0;
#endif
	dmp->dm_symtab.cts_data = NULL;
	dmp->dm_symtab.cts_size = 0;
	dmp->dm_symtab.cts_entsize = dmp->dm_ops == &dt_modops_64 ?
	    sizeof(Elf64_Sym) : sizeof(Elf32_Sym);

	dmp->dm_strtab.cts_name = ".strtab";
#ifndef HAVE_LIBCTF
	dmp->dm_strtab.cts_type = SHT_STRTAB;
	dmp->dm_strtab.cts_flags = 0;
	dmp->dm_strtab.cts_offset = 0;
#endif
	dmp->dm_strtab.cts_data = NULL;
	dmp->dm_strtab.cts_size = 0;
	dmp->dm_strtab.cts_entsize = 0;

	/*
	 * Now load the module's symbol and string table sections.
	 */
	if (dt_module_load_sect(dtp, dmp, &dmp->dm_symtab) == -1 ||
	    dt_module_load_sect(dtp, dmp, &dmp->dm_strtab) == -1) {
		dt_module_unload(dtp, dmp);
		return -1; /* dt_errno is set for us */
	}

	/*
	 * Allocate the hash chains and hash buckets for symbol name lookup.
	 * This is relatively simple since the symbol table is of fixed size
	 * and is known in advance.  We allocate one extra element since we
	 * use element indices instead of pointers and zero is our sentinel.
	 */
	dmp->dm_nsymelems =
	    dmp->dm_symtab.cts_size / dmp->dm_symtab.cts_entsize;

	dmp->dm_nsymbuckets = _dtrace_strbuckets;
	dmp->dm_symfree = 1;		/* first free element is index 1 */

	dmp->dm_symbuckets = malloc(sizeof(uint_t) * dmp->dm_nsymbuckets);
	dmp->dm_symchains = malloc(sizeof(dt_modsym_t) * dmp->dm_nsymelems + 1);

	if (dmp->dm_symbuckets == NULL || dmp->dm_symchains == NULL)
		goto oom;

	memset(dmp->dm_symbuckets, 0, sizeof(uint_t) * dmp->dm_nsymbuckets);
	memset(dmp->dm_symchains, 0, sizeof(dt_modsym_t) * dmp->dm_nsymelems
	    + 1);

	/*
	 * Iterate over the symbol table data buffer and insert each symbol
	 * name into the name hash if the name and type are valid.  Then
	 * allocate the address map, fill it in, and sort it.
	 */
	dmp->dm_asrsv = dmp->dm_ops->do_syminit(dmp);

	dt_dprintf("hashed %s [%s] (%u symbols)\n",
	    dmp->dm_name, dmp->dm_symtab.cts_name, dmp->dm_symfree - 1);

	if ((dmp->dm_asmap = malloc(sizeof(void *) * dmp->dm_asrsv)) == NULL)
		goto oom;

	dmp->dm_ops->do_symsort(dmp);

	dt_dprintf("sorted %s [%s] (%u symbols)\n",
	    dmp->dm_name, dmp->dm_symtab.cts_name, dmp->dm_aslen);

	dmp->dm_flags |= DT_DM_LOADED;
	return 0;
oom:
	dt_module_unload(dtp, dmp);
	return dt_set_errno(dtp, EDT_NOMEM);
}

/*
 * Get the CTF of a kernel module with CTF linked in.
 */
ctf_file_t *
dt_module_getctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	const char *parent;
	dt_module_t *pmp;
	ctf_file_t *pfp;
	int model;

	if (!(dmp->dm_flags & DT_DM_LOADED))
		if (dt_module_load(dtp, dmp) != 0)
			return NULL;

	if (dmp->dm_ctfp != NULL)
		return dmp->dm_ctfp;

	assert(!(dmp->dm_flags & DT_DM_CTF_ARCHIVED));

	if ((dmp->dm_ops == &dt_modops_64) || (dmp->dm_ops == NULL))
		model = CTF_MODEL_LP64;
	else
		model = CTF_MODEL_ILP32;

	/*
	 * If the data model of the module does not match our program data
	 * model, then do not permit CTF from this module to be opened and
	 * returned to the compiler.  If we support mixed data models in the
	 * future for combined kernel/user tracing, this can be removed.
	 */
	if (dtp->dt_conf.dtc_ctfmodel != model) {
		dt_set_errno(dtp, EDT_DATAMODEL);
		return NULL;
	}

	if ((dmp->dm_ctdata.cts_size == 0) ||
	    (dmp->dm_ctdata.cts_size == 1)) {
		dt_set_errno(dtp, EDT_NOCTF);
		return NULL;
	}

	if (dmp->dm_flags & DT_DM_KERNEL)
		dmp->dm_ctfp = ctf_bufopen(&dmp->dm_ctdata, NULL, NULL,
		    &dtp->dt_ctferr);
	else
		dmp->dm_ctfp = ctf_bufopen(&dmp->dm_ctdata,
		    &dmp->dm_symtab, &dmp->dm_strtab, &dtp->dt_ctferr);

	if (dmp->dm_ctfp == NULL) {
		dt_dprintf("ctf loading for module %s failed: error: %s\n",
		    dmp->dm_name, ctf_errmsg(dtp->dt_ctferr));
		dt_set_errno(dtp, EDT_CTF);
		return NULL;
	}

	ctf_setmodel(dmp->dm_ctfp, model);
	ctf_setspecific(dmp->dm_ctfp, dmp);

	if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
		if ((pmp = dt_module_create(dtp, parent)) == NULL ||
		    (pfp = dt_module_getctf(dtp, pmp)) == NULL) {
			if (pmp == NULL)
				dt_set_errno(dtp, EDT_NOMEM);
			goto err;
		}

		if (ctf_import(dmp->dm_ctfp, pfp) == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(dmp->dm_ctfp);
			dt_set_errno(dtp, EDT_CTF);
			goto err;
		}
	}

	dt_dprintf("loaded CTF container for %s (%p)\n",
	    dmp->dm_name, (void *)dmp->dm_ctfp);

	return dmp->dm_ctfp;

err:
	ctf_close(dmp->dm_ctfp);
	dmp->dm_ctfp = NULL;
	return NULL;
}

/*ARGSUSED*/
static void
dt_module_unload(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	if (dmp->dm_ctfp != dtp->dt_shared_ctf)
		ctf_close(dmp->dm_ctfp);
	dmp->dm_ctfp = NULL;

	free(dmp->dm_ctdata_name);
	dmp->dm_ctdata_name = NULL;
	free(dmp->dm_ctdata_data);
	dmp->dm_ctdata_data = NULL;

	memset(&dmp->dm_ctdata, 0, sizeof(ctf_sect_t));
	memset(&dmp->dm_symtab, 0, sizeof(ctf_sect_t));
	memset(&dmp->dm_strtab, 0, sizeof(ctf_sect_t));

	free(dmp->dm_symbuckets);
	dmp->dm_symbuckets = NULL;

	free(dmp->dm_symchains);
	dmp->dm_symchains = NULL;

	free(dmp->dm_asmap);
	dmp->dm_asmap = NULL;

	dt_symtab_destroy(dmp->dm_kernsyms);
	dmp->dm_kernsyms = NULL;

	dmp->dm_symfree = 0;
	dmp->dm_nsymbuckets = 0;
	dmp->dm_nsymelems = 0;
	dmp->dm_asrsv = 0;
	dmp->dm_aslen = 0;

	free(dmp->dm_text_addrs);
	free(dmp->dm_data_addrs);
	dmp->dm_text_addrs = NULL;
	dmp->dm_data_addrs = NULL;
	dmp->dm_text_addrs_size = 0;
	dmp->dm_data_addrs_size = 0;

	dt_idhash_destroy(dmp->dm_extern);
	dmp->dm_extern = NULL;

	/*
	 * Built-in modules may be sharing their libelf handle with other
	 * modules, so should not close it until its refcount falls to zero.
	 */
	if (dmp->dm_flags & DT_DM_BUILTIN) {
		if (--dtp->dt_ctf_elf_ref == 0) {
			elf_end(dtp->dt_ctf_elf);
			dtp->dt_ctf_elf = NULL;
		}
	} else
		elf_end(dmp->dm_elf);
	dmp->dm_elf = NULL;

	dmp->dm_flags &= ~DT_DM_LOADED;
}

void
dt_module_destroy(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	uint_t h = dt_strtab_hash(dmp->dm_name, NULL) % dtp->dt_modbuckets;
	dt_module_t *scan_dmp;
	dt_module_t *prev_dmp = NULL;

	dt_list_delete(&dtp->dt_modlist, dmp);
	assert(dtp->dt_nmods != 0);
	dtp->dt_nmods--;

	for (scan_dmp = dtp->dt_mods[h]; (scan_dmp != NULL) && (scan_dmp != dmp);
	     scan_dmp = scan_dmp->dm_next) {
		prev_dmp = scan_dmp;
	}
	if (prev_dmp == NULL)
		dtp->dt_mods[h] = dmp->dm_next;
	else
		prev_dmp->dm_next = dmp->dm_next;

	dt_module_unload(dtp, dmp);
	free(dmp);
}

/*
 * Insert a new external symbol reference into the specified module.  The new
 * symbol will be marked as undefined and is assigned a symbol index beyond
 * any existing cached symbols from this module.  We use the ident's di_data
 * field to store a pointer to a copy of the dtrace_syminfo_t for this symbol.
 */
dt_ident_t *
dt_module_extern(dtrace_hdl_t *dtp, dt_module_t *dmp,
    const char *name, const dtrace_typeinfo_t *tip)
{
	dtrace_syminfo_t *sip;
	dt_ident_t *idp;
	uint_t id;

	if (dmp->dm_extern == NULL && (dmp->dm_extern = dt_idhash_create(
	    "extern", NULL, dmp->dm_nsymelems, UINT_MAX)) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	if (dt_idhash_nextid(dmp->dm_extern, &id) == -1) {
		dt_set_errno(dtp, EDT_SYMOFLOW);
		return NULL;
	}

	if ((sip = malloc(sizeof(dtrace_syminfo_t))) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	idp = dt_idhash_insert(dmp->dm_extern, name, DT_IDENT_SYMBOL, 0, id,
	    _dtrace_symattr, 0, &dt_idops_thaw, NULL, dtp->dt_gen);

	if (idp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		free(sip);
		return NULL;
	}

	sip->object = dmp->dm_name;
	sip->name = idp->di_name;
	sip->id = idp->di_id;

	idp->di_data = sip;
	idp->di_ctfp = tip->dtt_ctfp;
	idp->di_type = tip->dtt_type;

	return idp;
}

const char *
dt_module_modelname(dt_module_t *dmp)
{
	if ((dmp->dm_ops == &dt_modops_64) || (dmp->dm_ops == NULL))
		return "64-bit";
	else
		return "32-bit";
}

/*
 * Exported interface to compare a GElf_Addr to an address range member, for
 * bsearch()ing.
 */
int
dtrace_addr_range_cmp(const void *addr_, const void *range_)
{
	const GElf_Addr *addr = addr_;
	const dtrace_addr_range_t *range = range_;

	if (*addr < range->dar_va)
		return -1;

	if (*addr >= range->dar_va + range->dar_size)
		return 1;

	return 0;
}

/*
 * Expand an address range and return the new entry.
 */
static dtrace_addr_range_t *
dtrace_addr_range_grow(dt_module_t *dmp, int is_text)
{
	dtrace_addr_range_t **range;
	dtrace_addr_range_t *new_range;
	dtrace_addr_range_t *final_range;
	size_t *size;

	if (is_text) {
		range = &dmp->dm_text_addrs;
		size = &dmp->dm_text_addrs_size;
	} else {
		range = &dmp->dm_data_addrs;
		size = &dmp->dm_data_addrs_size;
	}

	new_range = realloc(*range, sizeof(struct dtrace_addr_range) *
	    (*size+1));
	if (new_range == NULL)
		return NULL;

	*range = new_range;
	final_range = new_range + (*size);
	final_range->dar_va = 0;
	final_range->dar_size = 0;

	(*size)++;

	return final_range;
}

/*
 * Transform an nm(1)-style type field into an ELF info character.  This is the
 * rough inverse of code in nm(1) and kernel/module.c:elf_type().  (Extreme
 * accuracy is hardly called for in this application.)
 */
static char
sym_type_to_info(char info)
{
	int local = islower(info);
	int binding = local ? STB_LOCAL : STB_GLOBAL;
	int type;
	char lowinfo = tolower(info);

	switch (lowinfo) {
	case 't':
		type = STT_FUNC; break;
	case 'w':
		binding = STB_WEAK;
		type = STT_FUNC;
		break;
	case 'v':
		binding = STB_WEAK;
		type = STT_OBJECT;
		break;
	case 'a': /* a sort of data */
	case 'r':
	case 'g':
	case 'd':
	case 's':
	case 'b':
		type = STT_OBJECT; break;
	case 'c':
		type = STT_COMMON; break;

	case 'u': /* highly unlikely, kludge it */
	case '?':
	case 'n':
	default:
		type = STT_NOTYPE; break;
	}

	return GELF_ST_INFO(binding, type);
}
/*
 * Do all necessary post-creation initialization of a module of type
 * DT_DM_KERNEL.
 */
static int
dt_kern_module_init(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	dt_dprintf("initializing module %s\n", dmp->dm_name);

	dmp->dm_ops = NULL;
	dmp->dm_text_addrs = NULL;
	dmp->dm_data_addrs = NULL;
 	dmp->dm_text_addrs_size = 0;
	dmp->dm_data_addrs_size = 0;
	dmp->dm_flags |= DT_DM_KERNEL;

	return 0;
}

/*
 * Determine the location of a kernel module's CTF data.
 *
 * If the module is a CTF archive, also load it in.
 */
static void
dt_kern_module_find_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	dt_kern_path_t *dkpp = NULL;

	/*
	 * Module not already known as a kernel module?  It must be an unloaded
	 * one, since we do not support userspace modules yet.
         */
	if (!(dmp->dm_flags & DT_DM_KERNEL)) {
		dmp->dm_flags |= DT_DM_KERNEL;
		dmp->dm_flags |= DT_DM_KERN_UNLOADED;
		dt_kern_module_init(dtp, dmp);
	}

	/*
	 * Kernel modules' CTF can be in one of three places: the CTF archive,
	 * linked into the module (for out-of-tree modules or modules on older
	 * kernels), or the ctf.ko module (for built-in modules and the core
	 * kernel on older kernels).  The ctf.ko module is then instantiated in
	 * the dm_ctfp of many modules.  The corresponding dt_module for ctf.ko
	 * is named 'shared_ctf'.
	 *
	 * Shared modules share their dm_elf with dtp->dt_ctf_elf, and have
	 * the DT_DM_BUILTIN flag turned on.
	 *
	 * Note: before we call this function we cannot distinguish between a
	 * non-loaded kernel module and a userspace module.  Neither have
	 * DT_DM_KERNEL turned on: the only difference is that the latter has no
	 * entry in the kernpath hash.
	 *
	 * Modules in the CTF archive are simpler: they just pull their CTF
	 * straight out of dt_ctfa, as needed.
	 *
	 * Check for a CTF archive containing the specified module.
	 */
	if (dtp->dt_ctfa == NULL) {
		char *ctfa_name;
		char *to;

		/* use user provided CTF archive. */
		if (dtp->dt_ctfa_path == NULL) {
			ctfa_name = malloc(strlen(dtp->dt_module_path) +
			    strlen("/kernel/vmlinux.ctfa") + 1);
			to = stpcpy(ctfa_name, dtp->dt_module_path);
			stpcpy(to, "/kernel/vmlinux.ctfa");
		} else {
			ctfa_name = dtp->dt_ctfa_path;
		}

		if ((dtp->dt_ctfa = ctf_arc_open(ctfa_name,
			    &dtp->dt_ctferr)) == NULL) {
			dt_dprintf("Cannot open CTF archive %s: %s; looking "
			    "for in-module CTF instead.\n", ctfa_name,
			    ctf_errmsg(dtp->dt_ctferr));
		}

		if (dtp->dt_ctfa != NULL) {
			/*
			 * Load in the shared CTF immediately.
			 */
			dtp->dt_shared_ctf = ctf_arc_open_by_name(dtp->dt_ctfa,
			    "shared_ctf", &dtp->dt_ctferr);

			if (dtp->dt_shared_ctf == NULL) {
				dt_dprintf("Cannot get shared CTF from archive %s: %s.",
				    ctfa_name, ctf_errmsg(dtp->dt_ctferr));
				ctf_arc_close(dtp->dt_ctfa);
				dtp->dt_ctfa = NULL;
			} else {
				dt_dprintf("Loaded shared CTF from archive %s.\n",
				    ctfa_name);
			}
		}

		if (dtp->dt_ctfa_path == NULL)
			free(ctfa_name);
	}

	if (dtp->dt_ctfa != NULL) {
		dt_dprintf("Loading CTF for module %s from archive.\n",
		    dmp->dm_name);

		dmp->dm_ctfp = ctf_arc_open_by_name(dtp->dt_ctfa,
		    dmp->dm_name, &dtp->dt_ctferr);

		if (dmp->dm_ctfp == NULL) {
			dt_dprintf("Cannot open CTF for module %s in CTF "
			    "archive: %s; looking for out-of-tree module.\n",
			    dmp->dm_name, ctf_errmsg(dtp->dt_ctferr));
		}
	}

	if (dmp->dm_ctfp != NULL) {
		const char *parent;

		/*
		 * Initialize other stuff around the CTF.  Much of it
		 * (e.g. dm_ctdata_*) is not populated, but some stuff still
		 * needs to be done.
		 *
		 * We create a DTrace module for the parent module if it is not
		 * already loaded and if it is not the shared CTF, which doesn't
		 * need a module because it's already special-cased and has no
		 * associated kernel module in any case.
		 */
		dmp->dm_flags |= DT_DM_CTF_ARCHIVED;
		ctf_setspecific(dmp->dm_ctfp, dmp);

		if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
			if (strcmp(parent, "shared_ctf") == 0)
				ctf_import(dmp->dm_ctfp, dtp->dt_shared_ctf);
			else {
				dt_module_t *pmp;
				ctf_file_t *pfp;

				if (((pmp = dt_module_create(dtp,
						parent)) != NULL) &&
				    ((pfp = dt_module_getctf(dtp,
					    pmp)) != NULL))
					ctf_import(dmp->dm_ctfp, pfp);
			}
			dt_dprintf("loaded CTF container for %s (%p)\n",
			    dmp->dm_name, (void *)dmp->dm_ctfp);
		}
	}

	/*
	 * No CTF archive, module not present in it, or module not loaded so
	 * we'll need its symbol table later on.  Check for a standalone module.
	 */
	if ((dmp->dm_ctfp == NULL) || (dmp->dm_flags & DT_DM_KERN_UNLOADED)) {
		dkpp = dt_kern_path_lookup_by_name(dtp, dmp->dm_name);

		/*
		 * Not a kernel module at all?  That's quite acceptable: just
		 * return.
		 */
		if (!(dmp->dm_flags & DT_DM_KERNEL))
			return;

		if (!dkpp) {
			dkpp = dt_kern_path_lookup_by_name(dtp, "ctf");

			/*
			 * With no ctf.ko or CTF archive, we are in real
			 * trouble. Likely we have no modules at all: CTF lookup
			 * cannot work.  At any rate, the module path must
			 * remain unknown.
			 */
			if (!dkpp) {
				dt_dprintf("No ctf.ko\n");
				return;
			}

			dmp->dm_flags |= DT_DM_BUILTIN;
		}

		/*
		 * Now load the CTF from here.
		 *
		 * Loaded kernel modules need only the appropriately-named CTF
		 * section from the dm_file (which will be ctf.ko for built-in
		 * modules and the kernel proper).  Builtin kernel modules have
		 * a section name that varies per-module: the shared type
		 * repository has a unique name of its own: all other modules
		 * have a constant name.
		 */
	}

	/*
	 * This is definitely a kernel module with a file on disk (though
	 * perhaps not a loaded one).
	 */

	if (dkpp != NULL) {
		if (strcmp(dmp->dm_name, "shared_ctf") == 0)
			/*
			 * ctf.ko itself is a special case: it contains no code
			 * or types of its own, and is loaded purely to force
			 * allocation of a dt_module for it into which we can
			 * load the shared types.  We pretend that it is
			 * built-in and transform its name to 'shared_ctf', in
			 * order to load the CTF data for it from the
			 * .ctf.shared_ctf section where the shared types are
			 * found, rather than .ctf, which is empty (as it is for
			 * all modules that contain no types of their own).
			 */
			dmp->dm_flags |= DT_DM_BUILTIN & DT_DM_SHARED;
		else
			strlcpy(dmp->dm_file, dkpp->dkp_path,
			    sizeof(dmp->dm_file));
	}
}

/*
 * Update our module cache.  For each line, create or
 * populate the dt_module_t for this module (if necessary), extend its address
 * ranges as needed, and add the symbol in this line to the module's kernel
 * symbol table.
 *
 * If we return non-NULL, we might have a changing file, probably due
 * to module unloading during read.  Perhaps this case should trigger a retry.
 */
static int
dt_modsym_update(dtrace_hdl_t *dtp, const char *line, int flag)
{
	static int kernel_flag = 1;
	static dt_module_t *last_dmp = NULL;
	static int last_sym_text = -1;

	GElf_Addr sym_addr;
	long long unsigned sym_size = 1;
	char sym_type;
	int sym_text;
	dt_module_t *dmp;
	dtrace_addr_range_t *range = NULL;
	char sym_name[KSYM_NAME_MAX];
	char mod_name[PATH_MAX] = "vmlinux]";	/* note trailing ] */
	int skip = 0;

	/*
	 * Read symbol.
	 */

	if ((line[0] == '\n') || (line[0] == 0))
		return 0;

	if (flag == 0) {
		if (sscanf(line, "%llx %llx %c %s [%s",
		    (long long unsigned *)&sym_addr,
		    (long long unsigned *)&sym_size,
		    &sym_type, sym_name, mod_name) < 4) {
		    dt_dprintf("malformed /proc/kallmodsyms line: %s\n", line);
		    return EDT_CORRUPT_KALLSYMS;
		}
	} else {
		if (sscanf(line, "%llx %c %s [%s",
		    (long long unsigned *)&sym_addr,
		    &sym_type, sym_name, mod_name) < 3) {
		    dt_dprintf("malformed /proc/kallsyms line: %s\n", line);
		    return EDT_CORRUPT_KALLSYMS;
		}
	}

	sym_text = (sym_type == 't') || (sym_type == 'T')
	     || (sym_type == 'w') || (sym_type == 'W');
	mod_name[strlen(mod_name)-1] = '\0';	/* chop trailing ] */

	if (strcmp(mod_name, "bpf") == 0)
		return 0;

	/*
	 * Symbols of "absolute" type are typically defined per CPU.
	 * Their "addresses" here are very low and are actually offsets.
	 * Drop these symbols.
	 */
	if ((sym_type == 'a') || (sym_type == 'A'))
		return 0;

	/*
	 * The file starts with kernel (and built-in-module) symbols.
	 * The last kernel address is expected to have the name "_end".
	 * (There might also be a symbol "__brk_limit" with that address.)
	 * Thereafter, symbols will belong to loadable modules.
	 *
	 * kernel_flag==+1 means normal kernel (and built-in-module) symbols
	 * kernel_flag==-1 means loadable-module symbols
	 * kernel_flag==0 is an odd in-between case for the section markers
	 *   (they signal the imminent end of the kernel section)
	 */

	if ((strcmp(sym_name, "_end") == 0) ||
	    (strcmp(sym_name, "__brk_limit") == 0))
		kernel_flag = 0;
	else if (kernel_flag == 0)
		kernel_flag = -1;

#define KERNEL_FLAG_INIT_SCRATCH 0x80
	/*
	 * Another odd case is the .init.scratch section introduced by e1bfa87
	 * ("x86/mm: Create a workarea in the kernel for SME early encryption"),
	 * which appears in 5.2-rc6.  We ignore this section by setting the
	 * KERNEL_FLAG_INIT_SCRATCH flag in kernel_flag.
	 */
	if (strcmp(sym_name, "__init_scratch_begin") == 0) {
		kernel_flag |= KERNEL_FLAG_INIT_SCRATCH;
		return 0;
	} else if (strcmp(sym_name, "__init_scratch_end") == 0) {
		kernel_flag &= ~ KERNEL_FLAG_INIT_SCRATCH;
		return 0;
	} else if (kernel_flag & KERNEL_FLAG_INIT_SCRATCH) {
		return 0;
	}
#undef KERNEL_FLAG_INIT_SCRATCH

	/*
	 * Special case: rename the 'ctf' module to 'shared_ctf': the
	 * parent-name lookup code presumes that names that appear in CTF's
	 * parent section are the names of modules, but the ctf module's CTF
	 * section is special-cased to contain the contents of the shared_ctf
	 * repository, not ctf.ko's types.
	 */
	if (strcmp(mod_name, "ctf") == 0)
		strcpy(mod_name, "shared_ctf");

	/*
	 * Get module.
	 */

	dmp = dt_module_lookup_by_name(dtp, mod_name);
	if (dmp == NULL) {
		int err;

		dmp = dt_module_create(dtp, mod_name);
		if (dmp == NULL)
			return EDT_NOMEM;

		err = dt_kern_module_init(dtp, dmp);
		if (err != 0)
			return err;
	}

	/*
	 * Add the symbol to the module's kernel symbol table.
	 *
	 * Some very voluminous and unuseful symbols are silently skipped, being
	 * used to update ranges but not added to the kernel symbol table.
	 * It doesn't matter much if this net is cast too wide, since we only
	 * care if a symbol is present if control flow or data lookups might
	 * pass through it while a probe fires, and that won't happen to any
	 * of these symbols.
	 */
#define strstarts(var, x) (strncmp(var, x, strlen (x)) == 0)
	if ((strstarts(sym_name, "__crc_")) ||
	    (strstarts(sym_name, "__ksymtab_")) ||
	    (strstarts(sym_name, "__kcrctab_")) ||
	    (strstarts(sym_name, "__kstrtab_")) ||
	    (strstarts(sym_name, "__param_")) ||
	    (strstarts(sym_name, "__syscall_meta__")) ||
	    (strstarts(sym_name, "__p_syscall_meta__")) ||
	    (strstarts(sym_name, "__event_")) ||
	    (strstarts(sym_name, "event_")) ||
	    (strstarts(sym_name, "ftrace_event_")) ||
	    (strstarts(sym_name, "types__")) ||
	    (strstarts(sym_name, "args__")) ||
	    (strstarts(sym_name, "__tracepoint_")) ||
	    (strstarts(sym_name, "__tpstrtab_")) ||
	    (strstarts(sym_name, "__tpstrtab__")) ||
	    (strstarts(sym_name, "__initcall_")) ||
	    (strstarts(sym_name, "__setup_")) ||
	    (strstarts(sym_name, "__pci_fixup_")))
		skip = 1;
#undef strstarts

	if (!skip) {
		if (dmp->dm_kernsyms == NULL)
			dmp->dm_kernsyms = dt_symtab_create();

		if (dmp->dm_kernsyms == NULL)
			return EDT_NOMEM;

		if (dt_symbol_insert(dmp->dm_kernsyms, sym_name, sym_addr, sym_size,
			sym_type_to_info(sym_type)) == NULL)
			return EDT_NOMEM;
	}

	/*
	 * Expand the appropriate address range for this module.
	 * Picking the right range is easy, but done differently
	 * for loadable modules versus built-in modules (and kernel).
	 */

	if (sym_size == 0)
		return 0;

	if (kernel_flag >= 0) {
		/*
		 * The kernel and built-in modules are in address order.
		 */
		if (dmp == last_dmp && sym_text == last_sym_text) {
			if (sym_text)
				range = &dmp->dm_text_addrs
				    [dmp->dm_text_addrs_size - 1];
			else
				range = &dmp->dm_data_addrs
				    [dmp->dm_data_addrs_size - 1];
			range->dar_size = sym_addr + sym_size - range->dar_va;
		} else {
			last_dmp = dmp;
			last_sym_text = sym_text;
		}
	} else {
		/*
		 * Each loadable module has only one text and one data range.
		 */

		if (sym_text)
			range = dmp->dm_text_addrs;
		else
			range = dmp->dm_data_addrs;

		if (range) {
			GElf_Addr end;
			end = range->dar_va + range->dar_size;
			if (range->dar_va > sym_addr)
				range->dar_va = sym_addr;
			if (end < sym_addr + sym_size)
				end = sym_addr + sym_size;
			range->dar_size = end - range->dar_va;
		}

	}

	/*
	 * If there is no range to expand, create a new one.
	 */

	if (range == NULL) {
		range = dtrace_addr_range_grow(dmp, sym_text);
		if (range == NULL)
			return EDT_NOMEM;
		range->dar_va = sym_addr;
		range->dar_size = sym_size;
	}

	return 0;
}

/*
 * Unload all the loaded modules and then refresh the module cache with the
 * latest list of loaded modules and their address ranges.
 */
int
dtrace_update(dtrace_hdl_t *dtp)
{
	dt_module_t *dmp;
	FILE *fd;
	int flag = 0;

	for (dmp = dt_list_next(&dtp->dt_modlist);
	    dmp != NULL; dmp = dt_list_next(dmp))
		dt_module_unload(dtp, dmp);

	/*
	 * Note all the symbols currently loaded into the kernel's address
	 * space and construct modules with appropriate address ranges from
	 * each.
	 */
	if ((fd = fopen("/proc/kallmodsyms", "r")) == NULL &&
	    (fd = fopen("/proc/kallsyms", "r")) != NULL)
			flag = 1;
	if (fd != NULL) {
		char *line = NULL;
		size_t line_n = 0;
		while ((getline(&line, &line_n, fd)) > 0)
			if (dt_modsym_update(dtp, line, flag) != 0) {
				/* TODO: waiting on a warning infrastructure */
				dt_dprintf("warning: module CTF loading failed"
				    " on %s line %s\n",
				    flag ? "kallsyms" : "kallmodsyms", line);
				break; /* no hope of (much) CTF */
			}
		free(line);
		fclose(fd);
	} else {
		/* TODO: waiting on a warning infrastructure */
		dt_dprintf("warning: /proc/kallmodsyms is not "
		    "present: consider setting -x procfspath\n");
		dt_dprintf("warning: module CTF loading failed\n");
	}

	/*
	 * Work over all modules, now they are (hopefully) fully populated.
	 */
	for (dmp = dt_list_next(&dtp->dt_modlist); dmp != NULL;
	    dmp = dt_list_next(dmp)) {
		if (dmp->dm_kernsyms != NULL) {
			dt_symtab_sort(dmp->dm_kernsyms, flag);
			dt_symtab_purge(dmp->dm_kernsyms);
			dt_symtab_pack(dmp->dm_kernsyms);
		}
	}

	/*
	 * Look up all the macro identifiers and set di_id to the latest value.
	 * This code collaborates with dt_lex.l on the use of di_id.  We will
	 * need to implement something fancier if we need to support non-ints.
	 */
	dt_idhash_lookup(dtp->dt_macros, "egid")->di_id = getegid();
	dt_idhash_lookup(dtp->dt_macros, "euid")->di_id = geteuid();
	dt_idhash_lookup(dtp->dt_macros, "gid")->di_id = getgid();
	dt_idhash_lookup(dtp->dt_macros, "pid")->di_id = getpid();
	dt_idhash_lookup(dtp->dt_macros, "pgid")->di_id = getpgid(0);
	dt_idhash_lookup(dtp->dt_macros, "ppid")->di_id = getppid();
	dt_idhash_lookup(dtp->dt_macros, "sid")->di_id = getsid(0);
	dt_idhash_lookup(dtp->dt_macros, "uid")->di_id = getuid();

	/*
	 * Cache the pointers to the module representing the shared kernel
	 * symbols in the dtrace client handle.  (This is the same as 'genunix'
	 * in Solaris.)
	 */
	dtp->dt_exec = dt_module_lookup_by_name(dtp, "vmlinux");

	/*
	 * If this is the first time we are initializing the module list,
	 * shuffle the modules for vmlinux and the shared ctf to the front of
	 * the module list.  We do this so that type and symbol queries
	 * encounter vmlinux and thereby optimize for the common case in
	 * dtrace_lookup_by_name() and dtrace_lookup_by_type(), below.  The
	 * dtrace module is also shuffled to the front so that types used only,
	 * or largely, by DTrace translators and the DTrace module are resolved
	 * quickly.  (There are a few of these, mostly Solaris-compatibility
	 * types such as caddr_t.)
	 */
	if (dtp->dt_cdefs == NULL && dtp->dt_ddefs == NULL) {
		dt_module_shuffle_to_start(dtp, "dtrace");
		dt_module_shuffle_to_start(dtp, "vmlinux");
	}

	return 0;
}

/*
 * Shuffle one module to the start of the module list.
 */
static void
dt_module_shuffle_to_start(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t *dmp = dt_module_lookup_by_name(dtp, name);

	if (!dmp)
		return;

	dt_list_delete(&dtp->dt_modlist, dmp);
	dt_list_prepend(&dtp->dt_modlist, dmp);
}

static dt_module_t *
dt_module_from_object(dtrace_hdl_t *dtp, const char *object)
{
	int err = EDT_NOMOD;
	dt_module_t *dmp;

	if (object == DTRACE_OBJ_EXEC)
		dmp = dtp->dt_exec;
	else if (object == DTRACE_OBJ_CDEFS)
		dmp = dtp->dt_cdefs;
	else if (object == DTRACE_OBJ_DDEFS)
		dmp = dtp->dt_ddefs;
	else {
		dmp = dt_module_create(dtp, object);
		err = EDT_NOMEM;
	}

	if (dmp == NULL)
		dt_set_errno(dtp, err);

	return dmp;
}

/*
 * Exported interface to look up a symbol by name.  We return the (possibly
 * partial) GElf_Sym and complete symbol information for the matching symbol.
 *
 * Only the st_info, st_value, and st_size fields of the GElf_Sym are guaranteed
 * to be populated: the st_shndx is populated but its only meaningful value is
 * SHN_UNDEF versus !SHN_UNDEF.
 *
 * XXX profile this: possibly introduce a symbol->name mapping for
 * first-matching-symbol across all modules.
 */
int
dtrace_lookup_by_name(dtrace_hdl_t *dtp, const char *object, const char *name,
    GElf_Sym *symp, dtrace_syminfo_t *sip)
{
	dt_module_t *dmp;
	dt_ident_t *idp;
	uint_t n;
	GElf_Sym sym;

	uint_t mask = 0; /* mask of dt_module flags to match */
	uint_t bits = 0; /* flag bits that must be present */

	if (object != DTRACE_OBJ_EVERY &&
	    object != DTRACE_OBJ_KMODS &&
	    object != DTRACE_OBJ_UMODS) {
		if ((dmp = dt_module_from_object(dtp, object)) == NULL)
			return -1; /* dt_errno is set for us */

		if (dt_module_load(dtp, dmp) == -1)
			return -1; /* dt_errno is set for us */
		n = 1;

	} else {
		if (object == DTRACE_OBJ_KMODS)
			mask = bits = DT_DM_KERNEL;
		else if (object == DTRACE_OBJ_UMODS)
			mask = DT_DM_KERNEL;

		dmp = dt_list_next(&dtp->dt_modlist);
		n = dtp->dt_nmods;
	}

	if (symp == NULL)
		symp = &sym;

	for (; n > 0; n--, dmp = dt_list_next(dmp)) {
		if ((dmp->dm_flags & mask) != bits)
			continue; /* failed to match required attributes */

		if (dt_module_load(dtp, dmp) == -1)
			continue; /* failed to load symbol table */

		if ((dmp->dm_flags & DT_DM_KERNEL) &&
		    (!(dmp->dm_flags & DT_DM_KERN_UNLOADED))) {
			dt_symbol_t *dt_symp;

			if (!dmp->dm_kernsyms)
				continue;

			dt_symp = dt_symbol_by_name(dmp->dm_kernsyms, name);
			if (!dt_symp)
				continue;

			if (sip != NULL) {
				sip->object = dmp->dm_name;
				sip->name = dt_symbol_name(dmp->dm_kernsyms,
							   dt_symp);
				sip->id = 0;	/* undefined */
			}
			dt_symbol_to_elfsym(dtp, dt_symp, symp);

			return 0;
		} else {
			uint_t id;

			if (dmp->dm_ops->do_symname(dmp, name, symp, &id) != NULL) {
				if (sip != NULL) {
					sip->object = dmp->dm_name;
					sip->name = (const char *)
					    dmp->dm_strtab.cts_data + symp->st_name;
					sip->id = id;
				}
				return 0;
			}
		}

		if (dmp->dm_extern != NULL &&
		    (idp = dt_idhash_lookup(dmp->dm_extern, name)) != NULL) {
			if (symp != &sym) {
				symp->st_name = (uintptr_t)idp->di_name;
				symp->st_info =
				    GELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
				symp->st_other = 0;
				symp->st_shndx = SHN_UNDEF;
				symp->st_value = 0;
				symp->st_size =
				    ctf_type_size(idp->di_ctfp, idp->di_type);
			}

			if (sip != NULL) {
				sip->object = dmp->dm_name;
				sip->name = idp->di_name;
				sip->id = idp->di_id;
			}

			return 0;
		}
	}

	return dt_set_errno(dtp, EDT_NOSYM);
}

/*
 * Exported interface to look up a symbol by address.  We return the (possibly
 * partial) GElf_Sym and complete symbol information for the matching symbol.
 *
 * Only the st_info, st_value, and st_size fields of the GElf_Sym are guaranteed
 * to be populated: the st_shndx is populated but its only meaningful value is
 * SHN_UNDEF versus !SHN_UNDEF.
 *
 */
int
dtrace_lookup_by_addr(dtrace_hdl_t *dtp, GElf_Addr addr,
    GElf_Sym *symp, dtrace_syminfo_t *sip)
{
	dt_module_t *dmp;
	uint_t id;
	const dtrace_vector_t *v = dtp->dt_vector;

	if (v != NULL)
		return v->dtv_lookup_by_addr(dtp->dt_varg, addr, symp, sip);

	for (dmp = dt_list_next(&dtp->dt_modlist); dmp != NULL;
	    dmp = dt_list_next(dmp)) {
		void *i;

		i = bsearch(&addr, dmp->dm_text_addrs, dmp->dm_text_addrs_size,
		    sizeof(struct dtrace_addr_range), dtrace_addr_range_cmp);

		if (i)
			break;

		i = bsearch(&addr, dmp->dm_data_addrs, dmp->dm_data_addrs_size,
		    sizeof(struct dtrace_addr_range), dtrace_addr_range_cmp);

		if (i)
			break;
	}

	if (dmp == NULL) {
		dt_dprintf("No module corresponds to %lx\n", addr);
		return dt_set_errno(dtp, EDT_NOSYMADDR);
	}

	if (dt_module_load(dtp, dmp) == -1)
		return -1; /* dt_errno is set for us */

	if (dmp->dm_flags & DT_DM_KERNEL) {
		dt_symbol_t *dt_symp;

		/*
		 * This is probably a non-loaded kernel module.  Looking
		 * anything up by address in this case is hopeless.
		 */
		if (!dmp->dm_kernsyms)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		dt_symp = dt_symbol_by_addr(dmp->dm_kernsyms, addr);

		if (!dt_symp)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		if (sip != NULL) {
		    sip->object = dmp->dm_name;
		    sip->name = dt_symbol_name(dmp->dm_kernsyms, dt_symp);
		    sip->id = 0;	/* undefined */
		}

		if (symp != NULL)
		    dt_symbol_to_elfsym(dtp, dt_symp, symp);

		return 0;
	} else {
		if (symp != NULL) {
			if (dmp->dm_ops->do_symaddr(dmp, addr, symp, &id) == NULL)
				return dt_set_errno(dtp, EDT_NOSYMADDR);
		}

		if (sip != NULL) {
			sip->object = dmp->dm_name;

			if (symp != NULL) {
				sip->name = (const char *)
				    dmp->dm_strtab.cts_data + symp->st_name;
				sip->id = id;
			} else {
				sip->name = NULL;
				sip->id = 0;
			}
		}
	}

	return 0;
}

int
dtrace_lookup_by_type(dtrace_hdl_t *dtp, const char *object, const char *name,
    dtrace_typeinfo_t *tip)
{
	dtrace_typeinfo_t ti;
	dt_module_t *dmp;
	int found = 0;
	ctf_id_t id;
	uint_t n;
	int justone;

	uint_t mask = 0; /* mask of dt_module flags to match */
	uint_t bits = 0; /* flag bits that must be present */

	if (object != DTRACE_OBJ_EVERY &&
	    object != DTRACE_OBJ_KMODS &&
	    object != DTRACE_OBJ_UMODS) {
		if ((dmp = dt_module_from_object(dtp, object)) == NULL)
			return -1; /* dt_errno is set for us */

		if (dt_module_load(dtp, dmp) == -1)
			return -1; /* dt_errno is set for us */
		n = 1;
		justone = 1;

	} else {
		if (object == DTRACE_OBJ_KMODS)
			mask = bits = DT_DM_KERNEL;
		else if (object == DTRACE_OBJ_UMODS)
			mask = DT_DM_KERNEL;

		dmp = dt_list_next(&dtp->dt_modlist);
		n = dtp->dt_nmods;
		justone = 0;
	}

	if (tip == NULL)
		tip = &ti;

	for (; n > 0; n--, dmp = dt_list_next(dmp)) {
		if ((dmp->dm_flags & mask) != bits)
			continue; /* failed to match required attributes */

		/*
		 * If we can't load the CTF container, continue on to the next
		 * module.  If our search was scoped to only one module then
		 * return immediately leaving dt_errno unmodified.
		 */
		if (dt_module_getctf(dtp, dmp) == NULL) {
			if (justone)
				return -1;
			continue;
		}

		/*
		 * Look up the type in the module's CTF container.  If our
		 * match is a forward declaration tag, save this choice in
		 * 'tip' and keep going in the hope that we will locate the
		 * underlying structure definition.  Otherwise just return.
		 */
		if ((id = ctf_lookup_by_name(dmp->dm_ctfp, name)) != CTF_ERR) {
			tip->dtt_object = dmp->dm_name;
			tip->dtt_ctfp = dmp->dm_ctfp;
			tip->dtt_type = id;

			if (ctf_type_kind(dmp->dm_ctfp, ctf_type_resolve(
			    dmp->dm_ctfp, id)) != CTF_K_FORWARD)
				return 0;

			found++;
		}
	}

	if (found == 0)
		return dt_set_errno(dtp, EDT_NOTYPE);

	return 0;
}

int
dtrace_symbol_type(dtrace_hdl_t *dtp, const GElf_Sym *symp,
    const dtrace_syminfo_t *sip, dtrace_typeinfo_t *tip)
{
	dt_module_t *dmp;
	int undefined = 0;

	tip->dtt_object = NULL;
	tip->dtt_ctfp = NULL;
	tip->dtt_type = CTF_ERR;

	if ((dmp = dt_module_lookup_by_name(dtp, sip->object)) == NULL)
		return dt_set_errno(dtp, EDT_NOMOD);

	if (dmp->dm_flags & DT_DM_KERNEL) {
		if (dmp->dm_flags & DT_DM_KERN_UNLOADED)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		if (dt_module_getctf(dtp, dmp) == NULL)
			return -1; /* errno is set for us */

		tip->dtt_ctfp = dmp->dm_ctfp;
		tip->dtt_type = ctf_lookup_variable(dmp->dm_ctfp, sip->name);

		if (tip->dtt_type == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(tip->dtt_ctfp);

			if (ctf_errno(tip->dtt_ctfp) == ECTF_NOTYPEDAT)
				undefined = 1;
			else
				return dt_set_errno(dtp, EDT_CTF);
		}

	} else if (symp->st_shndx == SHN_UNDEF) {
		undefined = 1;

	} else if (GELF_ST_TYPE(symp->st_info) != STT_FUNC) {
		if (dt_module_getctf(dtp, dmp) == NULL)
			return -1; /* errno is set for us */

		tip->dtt_ctfp = dmp->dm_ctfp;
		tip->dtt_type = ctf_lookup_by_symbol(dmp->dm_ctfp, sip->id);

		if (tip->dtt_type == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(tip->dtt_ctfp);
			return dt_set_errno(dtp, EDT_CTF);
		}

	} else {
		tip->dtt_ctfp = DT_FPTR_CTFP(dtp);
		tip->dtt_type = DT_FPTR_TYPE(dtp);
	}

	if (undefined && dmp->dm_extern != NULL) {
		dt_ident_t *idp =
		    dt_idhash_lookup(dmp->dm_extern, sip->name);

		if (idp == NULL)
			return dt_set_errno(dtp, EDT_NOSYM);

		tip->dtt_ctfp = idp->di_ctfp;
		tip->dtt_type = idp->di_type;
	}

	tip->dtt_object = dmp->dm_name;
	return 0;
}

static dtrace_objinfo_t *
dt_module_info(const dt_module_t *dmp, dtrace_objinfo_t *dto)
{
	dto->dto_name = dmp->dm_name;
	dto->dto_file = dmp->dm_file;
	dto->dto_flags = 0;

	if (dmp->dm_flags & DT_DM_KERNEL)
		dto->dto_flags |= DTRACE_OBJ_F_KERNEL;

	dto->dto_text_addrs = dmp->dm_text_addrs;
	dto->dto_text_addrs_size = dmp->dm_text_addrs_size;
	dto->dto_data_addrs = dmp->dm_data_addrs;
	dto->dto_data_addrs_size = dmp->dm_data_addrs_size;

	return dto;
}

int
dtrace_object_iter(dtrace_hdl_t *dtp, dtrace_obj_f *func, void *data)
{
	const dt_module_t *dmp = dt_list_next(&dtp->dt_modlist);
	dtrace_objinfo_t dto;
	int rv;

	for (; dmp != NULL; dmp = dt_list_next(dmp)) {
		if ((rv = (*func)(dtp, dt_module_info(dmp, &dto), data)) != 0)
			return rv;
	}

	return 0;
}

int
dtrace_object_info(dtrace_hdl_t *dtp, const char *object, dtrace_objinfo_t *dto)
{
	dt_module_t *dmp;

	if (object == DTRACE_OBJ_EVERY || object == DTRACE_OBJ_KMODS ||
	    object == DTRACE_OBJ_UMODS || dto == NULL)
		return dt_set_errno(dtp, EINVAL);

	if ((dmp = dt_module_from_object(dtp, object)) == NULL)
		return -1; /* dt_errno is set for us */

	if (dt_module_load(dtp, dmp) == -1)
		return -1; /* dt_errno is set for us */

	dt_module_info(dmp, dto);
	return 0;
}
