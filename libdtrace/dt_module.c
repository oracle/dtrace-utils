/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
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

static uint32_t
dt_module_hval(const dt_module_t *mod)
{
	return str2hval(mod->dm_name, 0);
}

static int
dt_module_cmp(const dt_module_t *p, const dt_module_t *q)
{
	return strcmp(p->dm_name, q->dm_name);
}

DEFINE_HE_STD_LINK_FUNCS(dt_module, dt_module_t, dm_he)

static void *
dt_module_del_mod(dt_module_t *head, dt_module_t *dmp)
{
	head = dt_module_del(head, dmp);
	dt_list_delete(&dmp->dm_dtp->dt_modlist, dmp);

	dt_module_unload(dmp->dm_dtp, dmp);
	free(dmp);

	return head;
}

static dt_htab_ops_t dt_module_htab_ops = {
	.hval = (htab_hval_fn)dt_module_hval,
	.cmp = (htab_cmp_fn)dt_module_cmp,
	.add = (htab_add_fn)dt_module_add,
	.del = (htab_del_fn)dt_module_del_mod,
	.next = (htab_next_fn)dt_module_next
};

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
	h = str2hval(name, 0) % dmp->dm_nsymbuckets;
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
	dt_module_t *dmp;

	if (!dtp->dt_mods) {
		dtp->dt_mods = dt_htab_create(dtp, &dt_module_htab_ops);
		if (!dtp->dt_mods)
			return NULL;
	}

	if ((dmp = dt_module_lookup_by_name(dtp, name)) != NULL)
		return dmp;

	if ((dmp = malloc(sizeof(dt_module_t))) == NULL)
		return NULL; /* caller must handle allocation failure */

	memset(dmp, 0, sizeof(dt_module_t));
	strlcpy(dmp->dm_name, name, sizeof(dmp->dm_name));
	if (dt_htab_insert(dtp->dt_mods, dmp) < 0) {
		free(dmp);
		return NULL;
	}
	dt_list_append(&dtp->dt_modlist, dmp);

	if (dtp->dt_conf.dtc_ctfmodel == CTF_MODEL_LP64)
		dmp->dm_ops = &dt_modops_64;
	else
		dmp->dm_ops = &dt_modops_32;
	dmp->dm_dtp = dtp;

	return dmp;
}

dt_module_t *
dt_module_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t tmpl;

	if (strlen(name) > (DTRACE_MODNAMELEN - 1))
		return NULL;

	if (!dtp->dt_mods)
		return NULL;			/* no modules yet */

	/* 'genunix' is an alias for 'vmlinux'. */

	if (strcmp(name, "genunix") == 0) {
		name = "vmlinux";
	}

	strcpy(tmpl.dm_name, name);
	return dt_htab_lookup(dtp->dt_mods, &tmpl);
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

	if (!dmp->dm_file[0] == '\0') {
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
		dt_htab_delete(dtp->dt_mods, dmp);
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
		dt_htab_delete(dtp->dt_mods, dmp);
		return dt_set_errno(dtp, EDT_ELFCLASS);
	}

	dt_dprintf("opened %d-bit module %s (%s)\n", bits, dmp->dm_name,
	    dmp->dm_file);

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

		dmp->dm_ctdata.cts_name = ".ctf";

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

	if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
		assert(strcmp(parent, "shared_ctf") == 0);
		if (ctf_import(dmp->dm_ctfp, dtp->dt_shared_ctf) < 0) {
			dt_dprintf("Importing of CTF for %s failed: %s\n",
			    dmp->dm_name, ctf_errmsg(ctf_errno(dmp->dm_ctfp)));
			dtp->dt_ctferr = ctf_errno(dmp->dm_ctfp);
			ctf_close(dmp->dm_ctfp);
			dmp->dm_ctfp = NULL;
			return NULL;
		}
	}

	ctf_setspecific(dmp->dm_ctfp, dmp);

	dt_dprintf("loaded CTF container for %s (%p)\n",
	    dmp->dm_name, (void *)dmp->dm_ctfp);

	return dmp->dm_ctfp;
}

/*ARGSUSED*/
static void
dt_module_unload(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	if (dmp->dm_ctfp != dtp->dt_shared_ctf)
		ctf_close(dmp->dm_ctfp);
	dmp->dm_ctfp = NULL;

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

	dt_symtab_destroy(dtp, dmp->dm_kernsyms);
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

	elf_end(dmp->dm_elf);
	dmp->dm_elf = NULL;

	dmp->dm_flags &= ~DT_DM_LOADED;
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
	 * Kernel modules' CTF can be in one of two places: the CTF archive or
	 * linked into the module (for out-of-tree modules).  The corresponding
	 * dt_module for the shared CTF, wherever found, is named 'shared_ctf'.
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
		 * We do not yet support parent CTF that is not the shared CTF
		 * from the CTF archive; this support will only ever be needed
		 * for userspace CTF, and for that we can always get the CTF
		 * from the same archive as the child dict's in any case (and it
		 * is done for us by libctf).
		 *
		 * Don't even try to keep CTF around if parent importing fails:
		 * CTF with its parent types sliced off is useless.
		 */
		if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
			assert(strcmp(parent, "shared_ctf") == 0);
			if (ctf_import(dmp->dm_ctfp, dtp->dt_shared_ctf) < 0) {
				dt_dprintf("Importing of CTF for %s "
				    "failed: %s\n", dmp->dm_name,
				    ctf_errmsg(ctf_errno(dmp->dm_ctfp)));
				dtp->dt_ctferr = ctf_errno(dmp->dm_ctfp);
				ctf_close(dmp->dm_ctfp);
				dmp->dm_ctfp = NULL;
				return;
			}
			dt_dprintf("loaded CTF container for %s (%p)\n",
			    dmp->dm_name, (void *)dmp->dm_ctfp);
		}

		dmp->dm_flags |= DT_DM_CTF_ARCHIVED;
		ctf_setspecific(dmp->dm_ctfp, dmp);
	}

	/*
	 * No CTF archive, module not present in it, or module not loaded so
	 * we'll need its symbol table later on.  Check for a standalone module.
	 */
	if ((dmp->dm_ctfp == NULL) || (dmp->dm_flags & DT_DM_KERN_UNLOADED)) {
		dt_kern_path_t *dkpp = NULL;

		dkpp = dt_kern_path_lookup_by_name(dtp, dmp->dm_name);

		/*
		 * Not found, or not a kernel module at all?  That's quite
		 * acceptable: just return.
		 */
		if (!dkpp || !(dmp->dm_flags & DT_DM_KERNEL))
			return;

		strlcpy(dmp->dm_file, dkpp->dkp_path, sizeof(dmp->dm_file));
	}
}

/*
 * We will use kernel_flag to track which symbols we are reading.
 *
 * /proc/kallmodsyms starts with kernel (and built-in-module) symbols.
 *
 * The last kernel address is expected to have the name "_end",
 * but there might also be a symbol "__brk_limit" with that address.
 * Set the KERNEL_FLAG_KERNEL_END flag while these addresses are read.
 *
 * Otherwise, symbols in /proc/kallmodsyms will normally belong to
 * loadable modules.  Set the KERNEL_FLAG_LOADABLE flag once these
 * symbols are reached.
 *
 * Another odd case is the .init.scratch section introduced by e1bfa87
 * ("x86/mm: Create a workarea in the kernel for SME early encryption"),
 * which appears in 5.2-rc6.  We ignore this section by setting the
 * KERNEL_FLAG_INIT_SCRATCH flag.
 */
#define KERNEL_FLAG_KERNEL_END 1
#define KERNEL_FLAG_LOADABLE 2
#define KERNEL_FLAG_INIT_SCRATCH 4
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
	static uint_t kernel_flag = 0;
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
	 * "__builtin__kprobes" is used as a module name for symbols for pages
	 * allocated for kprobes' purposes, even though it is not a module.
	 */
	if (strcmp(mod_name, "__builtin__kprobes") == 0)
		return 0;

	/*
	 * Symbols of "absolute" type are typically defined per CPU.
	 * Their "addresses" here are very low and are actually offsets.
	 * Drop these symbols.
	 */
	if ((sym_type == 'a') || (sym_type == 'A'))
		return 0;

	/*
	 * Skip over the .init.scratch section.
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

	if ((strcmp(sym_name, "_end") == 0) ||
	    (strcmp(sym_name, "__brk_limit") == 0))
		kernel_flag |= KERNEL_FLAG_KERNEL_END;
	else if (kernel_flag & KERNEL_FLAG_KERNEL_END)
		kernel_flag = KERNEL_FLAG_LOADABLE;

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
			dmp->dm_kernsyms = dt_symtab_create(dtp);

		if (dmp->dm_kernsyms == NULL)
			return EDT_NOMEM;

		if (dt_symbol_insert(dtp, dmp->dm_kernsyms, dmp, sym_name,
			sym_addr, sym_size, sym_type_to_info(sym_type)) == NULL)
			return EDT_NOMEM;
	}

	/*
	 * Expand the appropriate address range for this module.
	 * Picking the right range is easy, but done differently
	 * for loadable modules versus built-in modules (and kernel).
	 */

	if (sym_size == 0)
		return 0;

	if ((kernel_flag & KERNEL_FLAG_LOADABLE) == 0) {
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
#undef KERNEL_FLAG_KERNEL_END
#undef KERNEL_FLAG_LOADABLE
#undef KERNEL_FLAG_INIT_SCRATCH

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
		n = dt_htab_entries(dtp->dt_mods);
	}

	if (symp == NULL)
		symp = &sym;

	/*
	 * Search loaded kernel symbols first, iff searching all kernel symbols
	 * is requested.  This is only an optimization: there are (obscure)
	 * cases in which loaded kernel symbols might not end up in this hash
	 * (e.g. if a kernel symbol is duplicated in multiple modules, then one
	 * of them is unloaded).
	 */
	if ((object == DTRACE_OBJ_EVERY || object == DTRACE_OBJ_KMODS) &&
	    dtp->dt_kernsyms) {
		dt_symbol_t *dt_symp;

		if ((dt_symp = dt_symbol_by_name(dtp, name)) != NULL) {
			if (sip != NULL) {
				dt_module_t *tmp;

				tmp = dt_symbol_module(dt_symp);
				sip->object = tmp->dm_name;
				sip->name = dt_symbol_name(dt_symp);
				sip->id = 0;	/* undefined */
			}

			dt_symbol_to_elfsym(dtp, dt_symp, symp);
			return 0;
		}
	}

	/*
	 * Not found: search all modules, including non-kernel modules, loading
	 * them as needed.
	 */
	for (; n > 0; n--, dmp = dt_list_next(dmp)) {
		if ((dmp->dm_flags & mask) != bits)
			continue; /* failed to match required attributes */

		if (dt_module_load(dtp, dmp) == -1)
			continue; /* failed to load symbol table */

		/*
		 * If this is a kernel symbol, see if it's appeared.  If it has,
		 * it must belong to this module.
		 */
		if ((dmp->dm_flags & DT_DM_KERNEL) &&
		    (!(dmp->dm_flags & DT_DM_KERN_UNLOADED))) {
			dt_symbol_t *dt_symp;

			if (!dmp->dm_kernsyms)
				continue;

			dt_symp = dt_module_symbol_by_name(dtp, dmp, name);
			if (!dt_symp)
				continue;
			assert(dt_symbol_module(dt_symp) == dmp);

			if (sip != NULL) {
				sip->object = dmp->dm_name;
				sip->name = dt_symbol_name(dt_symp);
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
		    sip->name = dt_symbol_name(dt_symp);
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
		n = dt_htab_entries(dtp->dt_mods);
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

	if (undefined) {
		if (dmp->dm_extern == NULL)
			return dt_set_errno(dtp, EDT_NOSYM);

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
