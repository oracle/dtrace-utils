/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <port.h>
#include <setjmp.h>

#include <mutex.h>

#include <rtld_db.h>

#include "libproc.h"
#include "Pcontrol.h"

static map_info_t *object_to_map(struct ps_prochandle *, Lmid_t, const char *);
static map_info_t *object_name_to_map(struct ps_prochandle *,
    Lmid_t, const char *);
static GElf_Sym *sym_by_name(sym_tbl_t *, const char *, GElf_Sym *, uint_t *);
static file_info_t *file_info_new(struct ps_prochandle *, map_info_t *);
static int byaddr_cmp_common(GElf_Sym *a, char *aname, GElf_Sym *b, char *bname);
static void optimize_symtab(sym_tbl_t *);
static void Pbuild_file_symtab(struct ps_prochandle *, file_info_t *);
static map_info_t *Paddr2mptr(struct ps_prochandle *P, uintptr_t addr);
static int Pxlookup_by_name_internal(struct ps_prochandle *P, Lmid_t lmid,
    const char *oname, const char *sname, int fixup_load_addr, GElf_Sym *symp,
    prsyminfo_t *sip);

#define	DATA_TYPES	\
	((1 << STT_OBJECT) | (1 << STT_FUNC) | \
	(1 << STT_COMMON) | (1 << STT_TLS))
#define	IS_DATA_TYPE(tp)	(((1 << (tp)) & DATA_TYPES) != 0)


static ulong_t
string_hash(const char *key)
{
	ulong_t g, h = 0;
	const char *p;

	for (p = key; *p != '\0'; p++) {
		h = (h << 4) + *p;

		if ((g = (h & 0xf0000000)) != 0) {
			h ^= (g >> 24);
			h ^= g;
		}
	}

	return (h);
}

/*
 * Allocation function for a new file_info_t.
 */
static file_info_t *
file_info_new(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr;

	if ((fptr = calloc(1, sizeof (file_info_t))) == NULL)
		return (NULL);

	dt_list_append(&P->file_list, fptr);
	fptr->file_pname = strdup(mptr->map_pmap->pr_file->prf_mapname);
	fptr->file_dev = mptr->map_pmap->pr_dev;
	fptr->file_inum = mptr->map_pmap->pr_inum;
	mptr->map_file = fptr;
	fptr->file_map = -1;
	fptr->file_ref = 1;
	P->num_files++;

	return (fptr);
}

/*
 * Deallocation function for a zero-refcount file_info_t.
 *
 * (Not the inverse of file_info_new(), hence the _del() name rather than
 * _free().)
 */
static void
file_info_del(struct ps_prochandle *P, file_info_t *fptr)
{
	_dprintf("%s: dropping file info with zero refcount\n",
	    fptr->file_pname);

	dt_list_delete(&P->file_list, fptr);
	free(fptr->file_symtab.sym_byname);
	free(fptr->file_symtab.sym_byaddr);

	free(fptr->file_dynsym.sym_byname);
	free(fptr->file_dynsym.sym_byaddr);

	if (fptr->file_lo)
		free(fptr->file_lo->rl_scope);

	free(fptr->file_lo);
	free(fptr->file_lname);
	free(fptr->file_pname);
	free(fptr->file_symsearch);
	elf_end(fptr->file_elf);
	free(fptr);
	P->num_files--;
}

/*
 * Deallocate all file_info_t's with zero reference count.
 */
static void
file_info_purge(struct ps_prochandle *P)
{
	uint_t i, num_files = P->num_files;
	file_info_t *fptr;
	file_info_t *old_fptr = NULL;

	for (i = 0, fptr = dt_list_next(&P->file_list);
	     i < num_files; i++, old_fptr = fptr,
		 fptr = dt_list_next(fptr)) {
		if (old_fptr && old_fptr->file_ref == 0)
			file_info_del(P, old_fptr);
	}
	if (old_fptr && old_fptr->file_ref == 0)
		file_info_del(P, old_fptr);
}


/*
 * Deallocates all map_info_t, prmap_file_t, prmaps, and search paths from a
 * prochandle.  Does not free file_info_t's, except for their search path
 * components; use file_info_purge() for that.  Does not free the mapping array
 * itself because it will often be reused immediately.
 */
static void
mapping_purge(struct ps_prochandle *P)
{
	file_info_t *fptr;
	size_t i;

	for (i = 0; i < P->num_mappings; i++) {
		if ((fptr = P->mappings[i].map_file) != NULL) {
			fptr->file_ref--;
			fptr->file_map = -1;
		}
		free(P->mappings[i].map_pmap->pr_mapaddrname);
		free(P->mappings[i].map_pmap);
		P->mappings[i].map_pmap = NULL;
	}
	P->num_mappings = 0;

	for (i = 0; i < MAP_HASH_BUCKETS; i++) {
		prmap_file_t *prf;
		prmap_file_t *old_prf = NULL;

		for (prf = P->map_files[i]; prf != NULL;
		     old_prf = prf, prf = prf->prf_next) {
			free(old_prf);
			free(prf->prf_mappings);
			free(prf->prf_mapname);
		}
		free(old_prf);
	}

	memset(P->map_files, 0, sizeof (struct prmap_file *) * MAP_HASH_BUCKETS);

	for (i = 0, fptr = dt_list_next(&P->file_list);
	     i < P->num_files; i++, fptr = dt_list_next(fptr)) {
		if (fptr->file_symsearch) {
			free(fptr->file_symsearch);
			fptr->file_symsearch = NULL;
			fptr->file_nsymsearch = 0;
		}
	}

	P->map_exec = -1;
	P->map_ldso = -1;
}

/*
 * Called from Pcreate() and Pgrab() to initialize the symbol table state in the
 * new ps_prochandle.
 */
int
Psym_init(struct ps_prochandle *P)
{
	P->map_files = calloc(MAP_HASH_BUCKETS, sizeof (struct prmap_file_t *));
	if (!P->map_files) {
		_dprintf("Out of memory initializing map_files hash\n");
		return -ENOMEM;
	}
	return 0;
}

/*
 * The opposite of Psym_init().
 */
void
Psym_free(struct ps_prochandle *P)
{
	Preset_maps(P);
	free(P->map_files);
	P->map_files = NULL;
}

/*
 * Do those things that must be done on a Prelease().
 */
void
Psym_release(struct ps_prochandle *P)
{
	rd_delete(P->rap);
	P->rap = NULL;
}

/*
 * Given a process handle, find a corresponding prmap_file by file name, or NULL
 * if none.
 */
static prmap_file_t *Pprmap_file_by_name(struct ps_prochandle *P,
    const char *name)
{
	uint_t h = string_hash(name) % MAP_HASH_BUCKETS;
	prmap_file_t *prf;

	for (prf = P->map_files[h]; prf != NULL; prf = prf->prf_next)
		if (strcmp(prf->prf_mapname, name) == 0)
			return (prf);

	return NULL;
}

/*
 * Call-back function for librtld_db to iterate through all of its shared
 * libraries and determine their load object names and lmids.
 */
static int
map_iter(const rd_loadobj_t *lop, size_t num, void *prochandle)
{
	char buf[PATH_MAX];
	struct ps_prochandle *P = prochandle;
	map_info_t *mptr;
	file_info_t *fptr;
	size_t scopes_size;

	_dprintf("encountered rd object with dyn at %p\n", (void *)lop->rl_dyn);

	/*
	 * The first mptr is the executable itself: the second is the vdso.
	 */
	if (num == 0) {
		if (P->map_exec != -1)
			mptr = &P->mappings[P->map_exec];
		else {
			_dprintf("map_iter: executable mapping, but not found "
			    "in /proc/%i/maps\n", P->pid);
			return (1);
		}
	}
	else if (num == 1) {
		_dprintf("map_iter: skipping vdso\n");
		return (1);
	} else if (lop->rl_dyn == 0)
		/*
		 * No dynamic section: this cannot be anything we are interested
		 * in.
		 */
		return (1);
	else if ((mptr = Paddr2mptr(P, lop->rl_dyn)) == NULL) {
		_dprintf("map_iter: base address of %lu doesn't match any mapping\n", lop->rl_dyn);
		return (1);
	}

	if (mptr->map_file == NULL) {
		_dprintf("map_iter: no file_info_t for this mapping\n");
		return (1);
	}

	fptr = mptr->map_file;

	/*
	 * Allocate a new file_lo, or free its dynamically allocated interior
	 * structure if already allocated.
	 */
	if (fptr->file_lo != NULL) {
		free(fptr->file_lo->rl_scope);
	} else if ((fptr->file_lo = malloc(sizeof (rd_loadobj_t))) == NULL) {
		_dprintf("map_iter: failed to allocate rd_loadobj_t\n");
		return (1);
	}

	*fptr->file_lo = *lop;

	if (num == 0) {
		/*
		 * The load object name is populated as an empty string by
		 * the C library, so we use the file name from the mapping.
		 */
		if (fptr->file_lname == NULL) {
			fptr->file_lbase = NULL;
			if ((fptr->file_lname =
			     strdup(fptr->file_pname)) != NULL)
				fptr->file_lbase = basename(fptr->file_lname);
		}
	} else if (Pread_string(P, buf, sizeof (buf), lop->rl_nameaddr) >= 0) {
		if ((fptr->file_lname == NULL) ||
		    (strcmp(fptr->file_lname, buf) != 0) ||
		    (buf[0] != '\0')) {

			free(fptr->file_lname);
			fptr->file_lbase = NULL;
			if ((fptr->file_lname = strdup(buf)) != NULL)
				fptr->file_lbase = basename(fptr->file_lname);
		}
	} else {
		_dprintf("map_iter: failed to read string at %p\n",
		    (void *)lop->rl_nameaddr);
	}

	/*
	 * We must take a copy of rl_scope, since it is aggressively reused by
	 * rtld_db.
	 */
	scopes_size = lop->rl_nscopes * sizeof(uintptr_t);
	fptr->file_lo->rl_scope = malloc(scopes_size);

	if (!fptr->file_lo->rl_scope) {
		_dprintf("map_iter: failed to allocate raw symbol search "
		    "path\n");
		return (1);
	}
	memcpy(fptr->file_lo->rl_scope, lop->rl_scope, scopes_size);

	_dprintf("loaded rd object %s lmid %lx\n",
	    fptr->file_lname ? fptr->file_lname : "<NULL>", lop->rl_lmident);
	return (1);
}

/*
 * Compute the file_symsearch for a given file_info.
 *
 * This is expensive to construct, and is only needed for a few file_info
 * instances, so is lazily constructed here.
 */
static void
Pupdate_symsearch(struct ps_prochandle *P, struct file_info *fptr)
{
	volatile rd_loadobj_t scope_lo = {0};
	jmp_buf * volatile old_exec_jmp;
	jmp_buf **jmp_pad, this_exec_jmp;
	size_t i = 0;

	if (fptr->file_symsearch != NULL ||
	    fptr->file_lo == NULL ||
	    P == NULL)
		return;

	fptr->file_symsearch = calloc(fptr->file_lo->rl_nscopes,
	    sizeof (struct file_info *));

	/*
	 * Failure to allocate here is not as serious as it seems: symbol
	 * lookups will fall back to an inaccurate linear lookup (as inaccurate
	 * as GDB!).
	 */
	if (!fptr->file_symsearch) {
		_dprintf("Cannot allocate %li bytes for symbol search "
		    "path for library with soname %s\n",
		    fptr->file_lo->rl_nscopes * sizeof (struct file_info *),
		    fptr->file_lbase);
		return;
	}

	/*
	 * If we spot an exec() in this function, free the scopes array and
	 * rethrow (or just return, alternatively).
	 */
	jmp_pad = libproc_unwinder_pad(P);
	old_exec_jmp = *jmp_pad;
	if (setjmp(this_exec_jmp)) {
		free(scope_lo.rl_scope);
		if (old_exec_jmp)
			longjmp(*old_exec_jmp, 1);
		*jmp_pad = old_exec_jmp;

		return;
	}
	*jmp_pad = &this_exec_jmp;

	/*
	 * Simply skip scopes we can't read out.  There's no point making a fuss
	 * if we can't a few of them, since the target process may have mutated
	 * them since we read them in, and aborting is excessive.  This means
	 * we'll often allocate a little too much space. That's not a problem.
	 */
	for (i = 0; i < fptr->file_lo->rl_nscopes; i++) {
		map_info_t *mptr;

		if (rd_get_scope(P->rap, (rd_loadobj_t *) &scope_lo, fptr->file_lo,
			i) == NULL) {
			_dprintf("Cannot read scope %lu in symbol search path "
			    "for library with soname %s\n", i,
			    fptr->file_lbase);
			continue;
		}

		/*
		 * In map_iter(), we have special cases for the vdso (skipping
		 * it) and for the executable, because we wanted to get info on
		 * it even if it had no dynamic section (and thus no rl_dyn).
		 * Here, we will simply skip any scope we cannot find, so a
		 * special case for the vdso is redundant; and a special case
		 * for the executable is pointless because if it has no dynamic
		 * section we cannot look up symbols in it anyway.
		 *
		 * TODO: if/when we get .ldynsym support, this needs rethinking,
		 *       since that will not affect rl_dyn but will nonetheless
		 *       permit symbol lookup in the executable.
		 */

		if (((mptr = Paddr2mptr(P, fptr->file_lo->rl_dyn)) == NULL) ||
		    mptr->map_file == NULL)
			continue;

		fptr->file_symsearch[fptr->file_nsymsearch++] = mptr->map_file;
	}

	free(scope_lo.rl_scope);
	*jmp_pad = old_exec_jmp;
}

/*
 * Go through all the address space mappings, validating or updating
 * the information already gathered, or gathering new information.
 *
 * This function is only called when we suspect that the mappings have changed
 * because of rtld activity, or when a process is referenced for the first time.
 */
void
Pupdate_maps(struct ps_prochandle *P)
{
	char mapfile[PATH_MAX];
	char exefile[PATH_MAX + 10] = "";	/* strlen(" (deleted)") */
	FILE *fp;

	size_t old_num_mappings = P->num_mappings;
	size_t i = 0;
	char *fn = NULL;
	char *mapaddrname = NULL;
	char *line = NULL;
	size_t len;

	if (P->info_valid)
		return;

	if (P->state == PS_DEAD)
		return;

	_dprintf("Updating mappings for PID %i\n", P->pid);

	/*
	 * For now, just throw away and reconstruct all the mappings from
	 * scratch.  This is theoretically inefficient, as a dlopen() or
	 * dlclose() will normally change only a few mappings: but it is much
	 * simpler to implement than the alternatives, and it ends up being
	 * cheaper than expected because we can avoid having to resort any of
	 * the mappings arrays (since /proc/$pid/maps is always sorted by
	 * address).  We are left with allocator overhead and nothing else.
	 *
	 * Because it is much more expensive to recompute the file_info_t, we
	 * preserve them (with zero reference count) and reuse them where
	 * possible.
	 */
	mapping_purge(P);

	(void) snprintf(mapfile, sizeof (mapfile), "%s/%d/maps",
	    procfs_path, (int)P->pid);
	if ((fp = fopen(mapfile, "r")) == NULL) {
		Preset_maps(P);
		return;
	}

	snprintf(exefile, sizeof (exefile), "%s/%d/exe", procfs_path,
	    (int)P->pid);
	if ((len = readlink(exefile, exefile, sizeof (exefile))) > 0)
		exefile[len] = '\0';

	while (getline(&line, &len, fp) >= 0) {
		unsigned long laddr, haddr, offset;
		ino_t	inode;
		unsigned int major;
		unsigned int minor;
		char	perms[5];
		const char *first_space;
		map_info_t *mptr;
		prmap_file_t *prf;
		prmap_t *pmptr;
		struct prmap **new_prf_mappings;

		sscanf(line, "%lx-%lx %s %lx %x:%x %lu %ms",
		    &laddr, &haddr, perms, &offset, &major, &minor, &inode,
		    &fn);

		/*
		 * Skip anonymous mappings, and special mappings like the stack,
		 * heap, and vdso; also skip on OOM.
		 */
		first_space = strchr(line, ' ');
		mapaddrname = strndup(line, first_space - line);
		if ((fn == NULL) || (mapaddrname == NULL) || (fn[0] == '[')) {
			free(fn);
			free(mapaddrname);
			continue;
		}

		/*
		 * Allocate a new map_info, and see if we need to allocate a new
		 * map_file.  Expand the map_info region only if necessary.
		 * (This makes multiple sequential unmaps cheaper, though
		 * nothing we can do can make multiple sequential maps cheap.)
		 */

		if (P->num_mappings >= old_num_mappings) {
			map_info_t *mappings = realloc(P->mappings,
			    sizeof (struct map_info) * (P->num_mappings + 1));
			if (!mappings)
				goto err;
			P->mappings = mappings;
		}

		mptr = &P->mappings[P->num_mappings];
		memset(mptr, 0, sizeof (struct map_info));

		mptr->map_pmap = malloc(sizeof (struct prmap));
		if (!mptr->map_pmap)
			goto err;
		pmptr = mptr->map_pmap;
		memset(pmptr, 0, sizeof (struct prmap));

		if ((prf = Pprmap_file_by_name(P, fn)) == NULL) {
			uint_t h = string_hash(fn) % MAP_HASH_BUCKETS;

			if ((prf = malloc(sizeof (struct prmap_file))) == NULL) {
				free(mptr->map_pmap);
				goto err;
			}

			memset(prf, 0, sizeof(struct prmap_file));
			prf->prf_mapname = fn;
			prf->prf_next = P->map_files[h];
			P->map_files[h] = prf;
		}
		else {
			free(fn);
			fn = NULL;
		}

		new_prf_mappings = realloc(prf->prf_mappings,
		    (prf->prf_num_mappings + 1) * sizeof (struct prmap_t *));

		if (new_prf_mappings == NULL) {
			free(mptr->map_pmap);
			goto err;
		}

		prf->prf_mappings = new_prf_mappings;
		prf->prf_mappings[prf->prf_num_mappings] = pmptr;
		prf->prf_num_mappings++;

		pmptr->pr_vaddr = laddr;
		pmptr->pr_size = haddr - laddr;
		pmptr->pr_mapaddrname = mapaddrname;

		/*
		 * Both ld.so and the kernel follow the rule that the first
		 * executable mapping they establish is the primary text
		 * mapping, and the first writable mapping is the primary data
		 * mapping.
		 */
		if (perms[0] == 'r')
			pmptr->pr_mflags |= MA_READ;
		if (perms[1] == 'w') {
			pmptr->pr_mflags |= MA_WRITE;
			if (prf->prf_data_map == NULL)
				prf->prf_data_map = pmptr;
		}
		if (perms[2] == 'x') {
			char *basename = strrchr(prf->prf_mapname, '/');
			char *suffix = strrchr(prf->prf_mapname, '.');

			pmptr->pr_mflags |= MA_EXEC;

			/*
			 * The primary text mapping must correspond to an
			 * on-disk mapping somewhere (since we cannot mmap()
			 * nor create a file_info for anonymous mappings).
			 * This is universally true in any case.
			 */
			if ((prf->prf_text_map == NULL) &&
			    (prf->prf_mapname[0] == '/'))
				prf->prf_text_map = pmptr;

			/*
			 * Heuristic to recognize the dynamic linker.  Works
			 * for /lib, /lib64, and Debian multiarch as well as
			 * conventional /lib/ld-2.13.so style systems.	(All
			 * versions of glibc 2.x name their dynamic linker
			 * something like ld-*.so.)
			 *
			 * If this heuristic fails, object_name_to_map() can use
			 * the AT_BASE auxv entry to come up with another guess
			 * (though this is likely to be stymied by dynamic
			 * linker relocation for non-statically-linked
			 * programs).
			 */

			if (basename && suffix && P->map_ldso == -1 &&
			    !P->no_dyn &&
			    (strncmp(prf->prf_mapname, "/lib", 4) == 0) &&
			    (strncmp(basename, "/ld-", 4) == 0) &&
			    (strcmp(suffix, ".so") == 0))
				P->map_ldso = P->num_mappings;

			/*
			 * Recognize the executable mapping.
			 */

			if (exefile[0] != '\0' && P->map_exec == -1 &&
			    (strcmp(prf->prf_mapname, exefile) == 0))
				P->map_exec = P->num_mappings;
		}
		pmptr->pr_dev = makedev(major, minor);
		pmptr->pr_inum = inode;
		pmptr->pr_file = prf;

		/*
		 * We try to merge any file information we may have for existing
		 * mappings, to avoid having to rebuild the file info.
		 *
		 * This is quite expensive if we have a lot of mappings, so we
		 * avoid doing it for those mappings that cannot possibly
		 * correspond to on-disk files.	 (It is still not guaranteed
		 * that all our file_info_t's correspond to ELF files.)
		 */

		if (prf->prf_mapname[0] == '/') {
			file_info_t *fptr;

			for (i = 0, fptr = dt_list_next(&P->file_list);
			     i < P->num_files; i++, fptr = dt_list_next(fptr)) {

				if (fptr->file_dev == pmptr->pr_dev &&
				    fptr->file_inum == pmptr->pr_inum &&
				    (strcmp(fptr->file_pname,
					prf->prf_mapname) == 0)) {
					/*
					 * This mapping matches. Revive it.
					 */

					fptr->file_ref++;
					mptr->map_file = fptr;
					break;
				}
			}

			if ((mptr->map_file == NULL) &&
			    (mptr->map_file = file_info_new(P, mptr)) == NULL) {
				_dprintf("failed to allocate a new "
				    "file_info_t for %s\n", prf->prf_mapname);
				/*
				 * Keep going: we can still work out other
				 * mappings.
				 */
			}

			if (mptr->map_file &&
			    mptr->map_file->file_map == -1 &&
			    mptr->map_pmap->pr_file->prf_text_map == mptr->map_pmap)
				mptr->map_file->file_map = P->num_mappings;
		}

		_dprintf("Added mapping for %s: %lx(%lx)\n", prf->prf_mapname,
		    pmptr->pr_vaddr, pmptr->pr_size);
		P->num_mappings++;
	}

	/*
	 * Drop file_info_t's corresponding to closed mappings, which will still
	 * have a zero refcount.
	 */
	file_info_purge(P);

	/*
	 * Note that we need to iterate across all mappings and recompute their
	 * lmids and sonames when we do a lookup that might depend on any such
	 * thing.  (That is, pretty much all lookups other than PR_OBJ_LDSO.)
	 *
	 * We do this lazily not for efficiency's sake but because rtld_db
	 * itself does a name lookup of _rtld_global inside ld.so to determine
	 * whether link maps are consistent, and iteration across load objects
	 * requires link map consistency.
	 */

	P->info_valid = 1;

	if (!P->no_dyn)
		P->lmids_valid = 0;

	fclose(fp);
	free(line);

	return;

err:
	fclose(fp);
	free(fn);
	free(mapaddrname);
	free(line);
	Preset_maps(P);
	return;
}

/*
 * Iterate across all mappings and recompute their lmids and sonames.
 * (Mappings which the dynamic linker does not know about, such as that
 * for the dynamic linker itself, will be left in lmid 0 by default.
 * This is almost certainly correct, since lmid use is vanishingly rare
 * on Linux.)
 *
 * Don't do this if we know this is a statically linked binary, since
 * they have no analogue of lmids at all.  (If we are not sure, rd_new()
 * will compute it.  The rd_new() initialization process itself can call
 * back into symbol lookup if this turns out to be a statically linked
 * binary, to prevent unnecessary recursion.)
 */
static void
Pupdate_lmids(struct ps_prochandle *P)
{
	if (P->info_valid && !P->noninvasive &&
	    !P->no_dyn && !P->lmids_valid) {
		if (P->rap == NULL)
			P->rap = rd_new(P);

		if (P->rap != NULL)
			rd_loadobj_iter(P->rap, map_iter, P);

		P->lmids_valid = 1;
	}
}

/*
 * Update all of the mappings as if by Pupdate_maps(), and then forcibly cache
 * all of the symbol tables associated with all object files.
 */
void
Pupdate_syms(struct ps_prochandle *P)
{
	file_info_t *fptr;
	int i;

	if (P->state == PS_DEAD)
		return;

	P->info_valid = 0;
	Pupdate_maps(P);
	Pupdate_lmids(P);

	for (i = 0, fptr = dt_list_next(&P->file_list);
	     i < P->num_files; i++, fptr = dt_list_next(fptr))
		Pbuild_file_symtab(P, fptr);
}

/*
 * Return the librtld_db agent handle for the victim process.
 * The handle will become invalid at the next successful exec() and the
 * client (caller of proc_rd_agent()) must not use it beyond that point.
 * If the process is already dead or noninvasively grabbed, there's
 * nothing we can do.
 */
rd_agent_t *
Prd_agent(struct ps_prochandle *P)
{
	if (P->state == PS_DEAD)
		return (NULL);

	if (P->rap == NULL && !P->noninvasive) {
		Pupdate_maps(P);
		Pupdate_lmids(P);
	}
       return (P->rap);
}

/*
 * Return the prmap_t structure containing 'addr' (no restrictions on
 * the type of mapping).
 */
const prmap_t *
Paddr_to_map(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;

	if (P->state == PS_DEAD)
		return (NULL);

	if (!P->info_valid) {
		Pupdate_maps(P);
		Pupdate_lmids(P);
	}

	if ((mptr = Paddr2mptr(P, addr)) != NULL)
		return (mptr->map_pmap);

	return (NULL);
}

/*
 * Convert a full or partial load object name to the prmap_t for its
 * corresponding primary text mapping.
 */
const prmap_t *
Plmid_to_map(struct ps_prochandle *P, Lmid_t lmid, const char *name)
{
	map_info_t *mptr;

	if (P->state == PS_DEAD)
		return (NULL);

	if (name == PR_OBJ_EVERY)
		return (NULL); /* A reasonable mistake */

	if ((mptr = object_name_to_map(P, lmid, name)) != NULL)
		return (mptr->map_pmap);

	return (NULL);
}

const prmap_t *
Pname_to_map(struct ps_prochandle *P, const char *name)
{
	return (Plmid_to_map(P, PR_LMID_EVERY, name));
}

/*
 * We wouldn't need these if qsort(3C) took an argument for the callback...
 */
static mutex_t sort_mtx = DEFAULTMUTEX;
static char *sort_strs;
static GElf_Sym *sort_syms;

static int
byaddr_cmp_common(GElf_Sym *a, char *aname, GElf_Sym *b, char *bname)
{
	if (a->st_value < b->st_value)
		return (-1);
	if (a->st_value > b->st_value)
		return (1);

	/*
	 * Prefer the function to the non-function.
	 */
	if (GELF_ST_TYPE(a->st_info) != GELF_ST_TYPE(b->st_info)) {
		if (GELF_ST_TYPE(a->st_info) == STT_FUNC)
			return (-1);
		if (GELF_ST_TYPE(b->st_info) == STT_FUNC)
			return (1);
	}

	/*
	 * Prefer the weak or strong global symbol to the local symbol.
	 */
	if (GELF_ST_BIND(a->st_info) != GELF_ST_BIND(b->st_info)) {
		if (GELF_ST_BIND(b->st_info) == STB_LOCAL)
			return (-1);
		if (GELF_ST_BIND(a->st_info) == STB_LOCAL)
			return (1);
	}

	/*
	 * Prefer the symbol that doesn't begin with a '$' since compilers and
	 * other symbol generators often use it as a prefix.
	 */
	if (*bname == '$')
		return (-1);
	if (*aname == '$')
		return (1);

	/*
	 * Prefer the name with fewer leading underscores.
	 */
	while (*aname == '_' && *bname == '_') {
		aname++;
		bname++;
	}

	if (*bname == '_')
		return (-1);
	if (*aname == '_')
		return (1);

	/*
	 * Prefer the symbol with the smaller size.
	 */
	if (a->st_size < b->st_size)
		return (-1);
	if (a->st_size > b->st_size)
		return (1);

	/*
	 * All other factors being equal, fall back to lexicographic order.
	 */
	return (strcmp(aname, bname));
}

static int
byaddr_cmp(const void *aa, const void *bb)
{
	GElf_Sym *a = &sort_syms[*(uint_t *)aa];
	GElf_Sym *b = &sort_syms[*(uint_t *)bb];
	char *aname = sort_strs + a->st_name;
	char *bname = sort_strs + b->st_name;

	return (byaddr_cmp_common(a, aname, b, bname));
}

static int
byname_cmp(const void *aa, const void *bb)
{
	GElf_Sym *a = &sort_syms[*(uint_t *)aa];
	GElf_Sym *b = &sort_syms[*(uint_t *)bb];
	char *aname = sort_strs + a->st_name;
	char *bname = sort_strs + b->st_name;

	return (strcmp(aname, bname));
}

/*
 * Given a symbol index, look up the corresponding symbol from the
 * given symbol table.
 *
 * This function allows the caller to treat the symbol table as a single
 * logical entity even though there may be 2 actual ELF symbol tables
 * involved. See the comments in Pcontrol.h for details.
 */
static GElf_Sym *
symtab_getsym(sym_tbl_t *symtab, int ndx, GElf_Sym *dst)
{
	/* If index is in range of primary symtab, look it up there */
	if (ndx >= symtab->sym_symn_aux)
		return (gelf_getsym(symtab->sym_data_pri,
		    ndx - symtab->sym_symn_aux, dst));

	/* Not in primary: Look it up in the auxiliary symtab */
	return (gelf_getsym(symtab->sym_data_aux, ndx, dst));
}

void __attribute__((__used__))
debug_dump_symtab(sym_tbl_t *symtab, const char *description)
{
	uint_t i;

	_dprintf("Symbol table dump of %s:\n", description);
	for (i = 0; i < symtab->sym_symn; i++) {
		GElf_Sym sym;
		if (symtab_getsym(symtab, i, &sym) != NULL) {
			_dprintf("%i %s %lx(%li), section %i\n", i,
			    sym.st_name < symtab->sym_strsz ?
			    symtab->sym_strs + sym.st_name :
			    "[unnamed]", sym.st_value, sym.st_size,
			    sym.st_shndx);
		}
	}
}

void __attribute__((__used__))
debug_dump_status(struct ps_prochandle *P)
{
	char *status = Pget_proc_status(P->pid, "State");

	if (status)
		_dprintf("State:\t%s", status);

	free(status);
}

static void
optimize_symtab(sym_tbl_t *symtab)
{
	GElf_Sym *symp, *syms;
	uint_t i, *indexa, *indexb;
	size_t symn, strsz, count;

	if (symtab == NULL || symtab->sym_data_pri == NULL ||
	    symtab->sym_byaddr != NULL)
		return;

	symn = symtab->sym_symn;
	strsz = symtab->sym_strsz;

	symp = syms = malloc(sizeof (GElf_Sym) * symn);
	if (symp == NULL) {
		_dprintf("optimize_symtab: failed to malloc symbol array");
		return;
	}

	/*
	 * First record all the symbols into a table and count up the ones
	 * that we're interested in. We mark symbols as invalid by setting
	 * the st_name to an illegal value.
	 */
	for (i = 0, count = 0; i < symn; i++, symp++) {
		if (symtab_getsym(symtab, i, symp) != NULL &&
		    symp->st_name < strsz &&
		    IS_DATA_TYPE(GELF_ST_TYPE(symp->st_info)))
			count++;
		else
			symp->st_name = strsz;
	}

	/*
	 * Allocate sufficient space for both tables and populate them
	 * with the same symbols we just counted.
	 */
	symtab->sym_count = count;
	indexa = symtab->sym_byaddr = calloc(sizeof (uint_t), count);
	indexb = symtab->sym_byname = calloc(sizeof (uint_t), count);
	if (indexa == NULL || indexb == NULL) {
		_dprintf(
		    "optimize_symtab: failed to malloc symbol index arrays");
		symtab->sym_count = 0;
		if (indexa != NULL) {	/* First alloc succeeded. Free it */
			free(indexa);
			symtab->sym_byaddr = NULL;
		}
		free(syms);
		return;
	}
	for (i = 0, symp = syms; i < symn; i++, symp++) {
		if (symp->st_name < strsz)
			*indexa++ = *indexb++ = i;
	}

	/*
	 * Sort the two tables according to the appropriate criteria.
	 */
	(void) mutex_lock(&sort_mtx);
	sort_strs = symtab->sym_strs;
	sort_syms = syms;

	qsort(symtab->sym_byaddr, count, sizeof (uint_t), byaddr_cmp);
	qsort(symtab->sym_byname, count, sizeof (uint_t), byname_cmp);

	sort_strs = NULL;
	sort_syms = NULL;
	(void) mutex_unlock(&sort_mtx);

	free(syms);
}

/*
 * Build the symbol table for the given mapped file.
 */
static void
Pbuild_file_symtab(struct ps_prochandle *P, file_info_t *fptr)
{
	size_t i;

	GElf_Ehdr ehdr;

	volatile int fd = -1;
	Elf_Data *shdata;
	Elf_Scn *scn;
	Elf *elf = NULL;
	volatile Elf *velf = NULL;
	size_t nshdrs, shstrndx;
	int mapfilefd;
	int err;
	jmp_buf * volatile old_exec_jmp;
	jmp_buf **jmp_pad, this_exec_jmp;

	struct {
		GElf_Shdr c_shdr;
		Elf_Data *c_data;
		const char *c_name;
	} *cp, *cache = NULL;

	if (!fptr) /* no file */
		return;

	if (fptr->file_init)
		return;	/* We've already processed this file */

	/*
	 * Mark the file_info struct as having the symbol table initialized
	 * even if we fail below.  We tried once; we don't try again.
	 */
	fptr->file_init = 1;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		_dprintf("libproc ELF version is more recent than libelf\n");
		return;
	}

	/*
	 * If we spot an exec() in this function, free everything and rethrow.
	 */
	jmp_pad = libproc_unwinder_pad(P);
	old_exec_jmp = *jmp_pad;
	if (setjmp(this_exec_jmp)) {
		if (fd > -1)
			close(fd);
		if (velf)
			elf_end((Elf *) velf);
		fptr->file_dyn_base = 0;
		free(cache);
		fptr->file_elf = NULL;

		if (old_exec_jmp)
			longjmp(*old_exec_jmp, 1);
		*jmp_pad = old_exec_jmp;

		return;
	}
	*jmp_pad = &this_exec_jmp;

	if (P->state == PS_DEAD) {
		/*
		 * If we're not live, there is nothing we can do.  (This should
		 * never happen in DTrace anyway.)
		 */
		_dprintf("cannot work over dead process\n");
		goto bad;
	}

	if (fptr->file_map == -1) {
		/*
		 * No primary text mapping for this file?  OK, we can't use it
		 * for symbol lookup yet.
		 */
		_dprintf("no primary text mapping for %s yet\n", fptr->file_pname);
		goto bad;
	}

	/*
	 * Acquire an fd to this mapping.  This may require ptrace()ing, but
	 * first, try to use the upstream /proc/$pid/map_files interface.
	 * Then create the elf file and get the elf header and .shstrtab data
	 * buffer so we can process sections by name.  If we're 'grabbing'
	 * without ptracing, without a monitor thread, and without map_files
	 * support, try to open the ELF object directly, though this is less
	 * reliable and can fail in the presence of deleted files and running
	 * executables for which we don't have read permission.  If we can't
	 * even do that, there is no way for us to read any kind of symbol table
	 * out of this executable.
	 *
	 * Note: This Ptrace() call may trigger breakpoint handlers, which can
	 * look up addresses, which can call this function: so temporarily mark
	 * this file as not initialized, in case of such a recursive call, and
	 * drop out immediately afterwards if it is marked as done now.  The
	 * same is true of the Puntrace().  There is no danger of infinite
	 * recursion because once a breakpoint is being processed, the victim is
	 * no longer running and thus cannot trigger another breakpoint.
	 */
	if ((mapfilefd = Pmapfilefd(P)) > -1)
		fd = openat(mapfilefd,
		    P->mappings[fptr->file_map].map_pmap->pr_mapaddrname,
		    O_RDONLY);

	if ((!P->noninvasive) && (fd < 0)) {
		long pfd;

		fptr->file_init = 0;
		err = Ptrace(P, 1);
		if (fptr->file_init == 1) {
			if (err >= 0)
				Puntrace(P, 0);
			goto ret;
		}
		fptr->file_init = 1;

		/*
		 * Even this can fail: fd table overflow, for instance.
		 * Fall back, if so.
		 */
		if (err >= 0) {
			if (wrapped_ptrace(P, PTRACE_GETMAPFD, P->pid,
				P->mappings[fptr->file_map].map_pmap->pr_vaddr,
				&pfd) < 0)
				fd = -1;
			else
				fd = pfd;
		}

		fptr->file_init = 0;
		Puntrace(P, 0);
		if (fptr->file_init == 1) {
			close(fd);
			goto ret;
		}
		fptr->file_init = 1;
	}

	if (fd < 0) {
		struct stat s;

		if ((stat(fptr->file_pname, &s) < 0) ||
		    s.st_dev != fptr->file_dev ||
		    s.st_ino != fptr->file_inum ||
		    ((fd = open(fptr->file_pname, O_RDONLY)) < 0)) {
			_dprintf("%i: cannot open %s in non-ptrace()d "
			    "process: replaced file or open() error\n",
			    P->pid, fptr->file_pname);
			goto bad;
		}
	}

	/*
	 * Don't hold the fd open forever. (ELF_C_READ followed by
	 * elf_cntl(..., ELF_C_FDREAD) triggers assertion failures in elfutils
	 * at gelf_getshdr() time: ELF_C_READ_MMAP works around this.)
	 */
	if ((elf = elf_begin(fd, ELF_C_READ_MMAP, NULL)) == NULL ||
	    elf_cntl(elf, ELF_C_FDREAD) == -1 ||
	    elf_kind(elf) != ELF_K_ELF ||
	    gelf_getehdr(elf, &ehdr) == NULL ||
	    elf_getshdrnum(elf, &nshdrs) == -1 ||
	    elf_getshdrstrndx(elf, &shstrndx) == -1 ||
	    (scn = elf_getscn(elf, shstrndx)) == NULL ||
	    (shdata = elf_getdata(scn, NULL)) == NULL) {
		err = elf_errno();

		close(fd);
		_dprintf("failed to process ELF file %s: %s\n",
		    fptr->file_pname, (err == 0) ? "<null>" : elf_errmsg(err));
		goto bad;
	}
	velf = elf;
	close(fd);
	if ((cache = malloc(nshdrs * sizeof (*cache))) == NULL) {
		_dprintf("failed to malloc section cache for mapping of %s\n",
		    fptr->file_pname);
		goto bad;
	}

	_dprintf("processing ELF file %s\n", fptr->file_pname);
	fptr->file_etype = ehdr.e_type;
	fptr->file_elf = elf;
	fptr->file_shstrs = shdata->d_buf;
	fptr->file_shstrsz = shdata->d_size;

	/*
	 * Iterate through each section, caching its section header, data
	 * pointer, and name.  We use this for handling sh_link values below.
	 */
	for (cp = cache + 1, scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL;
	     cp++) {
		if (gelf_getshdr(scn, &cp->c_shdr) == NULL) {
			_dprintf("Pbuild_file_symtab: Failed to get section "
			    "header\n");
			goto bad; /* Failed to get section header */
		}

		if ((cp->c_data = elf_getdata(scn, NULL)) == NULL) {
			_dprintf("Pbuild_file_symtab: Failed to get section "
			    "data\n");
			goto bad; /* Failed to get section data */
		}

		if (cp->c_shdr.sh_name >= shdata->d_size) {
			_dprintf("Pbuild_file_symtab: corrupt section name");
			goto bad; /* Corrupt section name */
		}

		cp->c_name = (const char *)shdata->d_buf + cp->c_shdr.sh_name;
	}

	/*
	 * Now iterate through the section cache in order to locate info
	 * for the .symtab, .dynsym and .SUNW_ldynsym sections.
	 */
	for (i = 1, cp = cache + 1; i < nshdrs; i++, cp++) {
		GElf_Shdr *shp = &cp->c_shdr;

		if (shp->sh_type == SHT_SYMTAB || shp->sh_type == SHT_DYNSYM) {
			sym_tbl_t *symp = shp->sh_type == SHT_SYMTAB ?
			    &fptr->file_symtab : &fptr->file_dynsym;
			/*
			 * It's possible that the we already got the symbol
			 * table from the core file itself.  We'll just be
			 * replacing the symbol table we pulled out of the core
			 * file with an equivalent one.  In either case, this
			 * check isn't essential, but it's a good idea.
			 */
			if (symp->sym_data_pri == NULL) {
				_dprintf("Symbol table found for %s\n",
				    fptr->file_pname);
				symp->sym_data_pri = cp->c_data;
				symp->sym_symn +=
				    shp->sh_size / shp->sh_entsize;
				symp->sym_strs =
				    cache[shp->sh_link].c_data->d_buf;
				symp->sym_strsz =
				    cache[shp->sh_link].c_data->d_size;
				symp->sym_hdr_pri = cp->c_shdr;
				symp->sym_strhdr = cache[shp->sh_link].c_shdr;
			} else {
				_dprintf("Symbol table already there for %s\n",
				    fptr->file_pname);
			}
#ifdef LATER
		} else if (shp->sh_type == SHT_SUNW_LDYNSYM) {
			/* .SUNW_ldynsym section is auxiliary to .dynsym */
			if (fptr->file_dynsym.sym_data_aux == NULL) {
				_dprintf(".SUNW_ldynsym symbol table"
				    " found for %s\n",
				    fptr->file_pname);
				fptr->file_dynsym.sym_data_aux = cp->c_data;
				fptr->file_dynsym.sym_symn_aux =
				    shp->sh_size / shp->sh_entsize;
				fptr->file_dynsym.sym_symn +=
				    fptr->file_dynsym.sym_symn_aux;
				fptr->file_dynsym.sym_hdr_aux = cp->c_shdr;
			} else {
				_dprintf(".SUNW_ldynsym symbol table already"
				    " there for %s\n", fptr->file_pname);
			}
#endif
		}
	}

	/*
	 * At this point, we've found all the symbol tables we're ever going
	 * to find: the ones in the loop above and possibly the symtab that
	 * was included in the core file. Before we perform any lookups, we
	 * create sorted versions to optimize for lookups.
	 */
	optimize_symtab(&fptr->file_symtab);
	optimize_symtab(&fptr->file_dynsym);

	free(cache);

	/*
	 * Fill in the base address of the text mapping and entry point address
	 * for relocatable objects.
	 *
	 * If the object is a shared library or other dynamic object, set
	 * file_dyn_base to the dynamic linker's idea of its load bias, rather
	 * than engaging in a tiresome search through the object file.  ld.so
	 * itself never gets this treatment, since it relocates itself without
	 * setting its rl_diff_addr: we must always compute its base address the
	 * tiresome way.
	 */
	if (fptr->file_etype == ET_DYN &&
	    fptr->file_lo != NULL &&
	    fptr->file_map != P->map_ldso) {
	       fptr->file_dyn_base = fptr->file_lo->rl_diff_addr;
	       _dprintf("%s: ld.so says file_dyn_base is %lx\n",
		   fptr->file_pname, fptr->file_dyn_base);
	       goto ret;
	}

	size_t nphdrs;
	GElf_Addr lowest_vaddr = (GElf_Addr) -1;

	if (elf_getphdrnum(elf, &nphdrs) < 0)
		goto elf_bad_noaddr;

	for (i = 0; i < nphdrs; i++) {
		GElf_Phdr hdr;
		GElf_Phdr *phdr = gelf_getphdr(elf, i, &hdr);

		if (!phdr)
			goto elf_bad_noaddr;

		if (phdr->p_type == PT_LOAD &&
		    phdr->p_vaddr < lowest_vaddr)
			lowest_vaddr = phdr->p_vaddr;
	}

	if (lowest_vaddr == (GElf_Addr) -1) {
		_dprintf("%s: no loadable sections.\n", fptr->file_pname);
		goto elf_bad_noaddr;
	}

	fptr->file_dyn_base = P->mappings[fptr->file_map].map_pmap->pr_vaddr -
	    lowest_vaddr;

	_dprintf("setting file_dyn_base for %s to %lx, "
	    "from vaddr of %lx and lowest_addr of %lx\n",
	    fptr->file_pname, (long)fptr->file_dyn_base,
	    P->mappings[fptr->file_map].map_pmap->pr_vaddr,
	    lowest_vaddr);

ret:
	*jmp_pad = old_exec_jmp;
	return;

elf_bad_noaddr:
	/*
	 * Can't reliably derive the base address.  Symbol lookup is quite
	 * unlikely to work, but we can still try with a zero base address.
	 */
	err = elf_errno();

	_dprintf("failed to get base address for ELF file %s, symbol lookup "
	    "likely broken: %s\n", fptr->file_pname, (err == 0) ? "<null>" :
	    elf_errmsg(err));
	fptr->file_dyn_base = 0;
	free(cache);
	*jmp_pad = old_exec_jmp;
	return;

bad:
	free(cache);
	elf_end(elf);
	fptr->file_elf = NULL;
	*jmp_pad = old_exec_jmp;
}

/*
 * Given a process virtual address, return the index of the map_info_t
 * containing it.  If none found, return -1.
 */
static ssize_t
Paddr2idx(struct ps_prochandle *P, uintptr_t addr)
{
	int lo = 0;
	int hi = P->num_mappings - 1;
	int mid;
	map_info_t *mp;

	while (lo <= hi) {

		mid = (lo + hi) / 2;
		mp = &P->mappings[mid];

		/* check that addr is in [vaddr, vaddr + size) */
		if ((addr - mp->map_pmap->pr_vaddr) < mp->map_pmap->pr_size)
			return (mid);

		if (addr < mp->map_pmap->pr_vaddr)
			hi = mid - 1;
		else
			lo = mid + 1;
	}

	return -1;
}

/*
 * Given a process virtual address, return the map_info_t containing it.
 * If none found, return NULL.
 *
 * These addresses are not stable across calls to Pupdate_maps()!
 */
static map_info_t *
Paddr2mptr(struct ps_prochandle *P, uintptr_t addr)
{
	ssize_t mpidx = Paddr2idx(P, addr);

	if (mpidx == -1)
		return (NULL);

	return &P->mappings[mpidx];
}

/*
 * Given a shared object name, return the map_info_t for it.  If no matching
 * object is found, return NULL.  Normally, the link maps contain the full
 * object pathname, e.g. /usr/lib/libc.so.1.  We allow the object name to
 * take one of the following forms:
 *
 * 1. An exact match (i.e. a full pathname): "/usr/lib/libc.so.1"
 * 2. An exact basename match: "libc.so.1"
 * 3. An initial basename match up to a '.' suffix: "libc.so" or "libc"
 * 4. The literal string "a.out" is an alias for the executable mapping
 *
 * The third case is a convenience for callers and may not be necessary.
 *
 * As the exact same object name may be loaded on different link maps (see
 * dlmopen(3DL)), we also allow the caller to resolve the object name by
 * specifying a particular link map id.  If lmid is PR_LMID_EVERY, the
 * first matching name will be returned, regardless of the link map id.
 */
static map_info_t *
object_to_map(struct ps_prochandle *P, Lmid_t lmid, const char *objname)
{
	map_info_t *mp;
	file_info_t *fp;
	size_t objlen;
	uint_t i;

	/*
	 * If we have no rtld_db, then always treat a request as one for all
	 * link maps.
	 */
	if (P->rap == NULL)
		lmid = PR_LMID_EVERY;

	/*
	 * First pass: look for exact matches of the entire pathname, or
	 * basename (cases 1 and 2 above):
	 */
	for (i = 0, mp = P->mappings; i < P->num_mappings; i++, mp++) {

		if (mp->map_pmap->pr_file->prf_mapname[0] != '/' ||
		    (fp = mp->map_file) == NULL)
			continue;

		if (lmid != PR_LMID_EVERY &&
		    (fp->file_lo == NULL || lmid != fp->file_lo->rl_lmident))
			continue;

		/*
		 * If we match, return the primary text mapping, if there is
		 * one; if none (unlikely), just return the mapping we matched.
		 */
		if ((fp->file_pname && strcmp(fp->file_pname, objname) == 0) ||
		    (fp->file_lbase && strcmp(fp->file_lbase, objname) == 0) ||
		    (fp->file_lname && strcmp(fp->file_lname, objname) == 0))
			return (fp->file_map != -1 ? &P->mappings[fp->file_map] : mp);
	}

	objlen = strlen(objname);

	/*
	 * Second pass: look for partial matches (case 3 above):
	 */
	for (i = 0, mp = P->mappings; i < P->num_mappings; i++, mp++) {

		if (mp->map_pmap->pr_file->prf_mapname[0] != '/' ||
		    (fp = mp->map_file) == NULL)
			continue;

		if (lmid != PR_LMID_EVERY &&
		    (fp->file_lo == NULL || lmid != fp->file_lo->rl_lmident))
			continue;

		/*
		 * If we match, return the primary text mapping, if there is
		 * one; if none (unlikely), just return the mapping we matched.
		 */
		if (fp->file_lbase &&
		    (strncmp(fp->file_lbase, objname, objlen) == 0) &&
		    (fp->file_lbase[objlen] == '.'))
			return (fp->file_map != -1 ? &P->mappings[fp->file_map] : mp);
	}


	/*
	 * We allow "a.out" to always alias the executable, assuming this name
	 * was not in use for something else.
	 */
	if ((lmid == PR_LMID_EVERY || lmid == LM_ID_BASE) &&
	    (strcmp(objname, "a.out") == 0) && (P->map_exec != -1))
		return (&P->mappings[P->map_exec]);

	return (NULL);
}

static map_info_t *
object_name_to_map(struct ps_prochandle *P, Lmid_t lmid, const char *name)
{
	map_info_t *mptr;

	if (!P->info_valid) {
		Pupdate_maps(P);

		if (name != PR_OBJ_LDSO)
			Pupdate_lmids(P);
	}

	if (P->map_ldso == -1)
		P->map_ldso = Paddr2idx(P, Pgetauxval(P, AT_BASE));

	if (name == PR_OBJ_EXEC) {
		if (P->map_exec != -1)
			mptr = &P->mappings[P->map_exec];
		else
			mptr = NULL;
	} else if (name == PR_OBJ_LDSO) {
		if (P->map_ldso != -1)
			mptr = &P->mappings[P->map_ldso];
		else
			mptr = NULL;
	} else
		mptr = object_to_map(P, lmid, name);

	return (mptr);
}

/*
 * When two symbols are found by address, decide which one is to be preferred.
 */
static GElf_Sym *
sym_prefer(GElf_Sym *sym1, char *name1, GElf_Sym *sym2, char *name2)
{
	/*
	 * Prefer the non-NULL symbol.
	 */
	if (sym1 == NULL)
		return (sym2);
	if (sym2 == NULL)
		return (sym1);

	/*
	 * Defer to the sort ordering...
	 */
	return (byaddr_cmp_common(sym1, name1, sym2, name2) <= 0 ? sym1 : sym2);
}

/*
 * Look up a symbol by address in the specified symbol table, using a binary
 * search.
 *
 * Adjustment to 'addr' must already have been made for the
 * offset of the symbol if this is a dynamic library symbol table.
 */
static GElf_Sym *
sym_by_addr(sym_tbl_t *symtab, GElf_Addr addr, GElf_Sym *symp, uint_t *idp)
{
	GElf_Sym sym, osym;
	uint_t i, oid = 0, *byaddr = symtab->sym_byaddr;
	int min, max, mid, omid = 0, found = 0;

	if (symtab->sym_data_pri == NULL || symtab->sym_count == 0)
		return (NULL);

	min = 0;
	max = symtab->sym_count - 1;
	osym.st_value = 0;

	/*
	 * We can't return when we've found a match, we have to continue
	 * searching for the closest matching symbol.
	 */
	while (min <= max) {
		mid = (max + min) / 2;

		i = byaddr[mid];
		if ((symtab_getsym(symtab, i, &sym)) &&
		    (addr >= sym.st_value &&
			addr < sym.st_value + sym.st_size &&
			(!found || sym.st_value > osym.st_value))) {
			osym = sym;
			omid = mid;
			oid = i;
			found = 1;
		}

		if (addr < sym.st_value)
			max = mid - 1;
		else
			min = mid + 1;
	}

	if (!found)
		return (NULL);

	/*
	 * There may be many symbols with identical values so we walk
	 * backward in the byaddr table to find the best match.
	 */
	do {
		sym = osym;
		i = oid;

		if (omid == 0)
			break;

		oid = byaddr[--omid];
		symtab_getsym(symtab, oid, &osym);
	} while (addr >= osym.st_value &&
	    addr < sym.st_value + osym.st_size &&
	    osym.st_value == sym.st_value);

	*symp = sym;
	if (idp != NULL)
		*idp = i;
	return (symp);
}

/*
 * Look up a symbol by name in the specified symbol table, using a binary
 * search.
 */
static GElf_Sym *
sym_by_name(sym_tbl_t *symtab, const char *name, GElf_Sym *symp, uint_t *idp)
{
	char *strs = symtab->sym_strs;
	uint_t i, *byname = symtab->sym_byname;
	int min, mid, max, cmp;

	if (symtab->sym_data_pri == NULL || strs == NULL ||
	    symtab->sym_count == 0)
		return (NULL);

	min = 0;
	max = symtab->sym_count - 1;

	while (min <= max) {
		GElf_Sym *sym;
		mid = (max + min) / 2;

		i = byname[mid];
		sym = symtab_getsym(symtab, i, symp);
		if (sym == NULL)
			_dprintf("null sym %i!\n", i);

		if ((cmp = strcmp(name, strs + symp->st_name)) == 0) {
			if (idp != NULL)
				*idp = i;
			return (symp);
		}

		if (cmp < 0)
			max = mid - 1;
		else
			min = mid + 1;
	}

	return (NULL);
}

/*
 * State for the symbol search iterator.
 */
typedef struct sym_search_iter {
	enum { SSI_START, SSI_PATH, SSI_START_LINEAR, SSI_LINEAR, SSI_END } ssi_state;
	size_t path_index;
	file_info_t *fptr;
} sym_search_iter_t;

/*
 * An iterator over file_info_t's, scanning the symbol search path from a given
 * point, then every library in turn.  Symbols may be returned more than once.
 */
static file_info_t *
sym_search_next(struct ps_prochandle *P, file_info_t *fptr,
    sym_search_iter_t *state, int just_one)
{
	/*
	 * Trivial case first: just_one alternates between returning what is
	 * passed in, and returning NULL (to end the loop).
	 */
	if (just_one) {
		if (state->ssi_state == SSI_START) {
			state->ssi_state = SSI_END;
			return fptr;
		} else {
			state->ssi_state = SSI_START;
			return NULL;
		}
	}

	/*
	 * Start: first hit is always ourself.  Subsequently, use the path
	 * search, if possible, or a linear search otherwise.
	 */
	if (state->ssi_state == SSI_START) {
		state->fptr = fptr;
		state->path_index = 0;

		if (fptr->file_nsymsearch != 0)
			state->ssi_state = SSI_PATH;
		else
			state->ssi_state = SSI_START_LINEAR;

		return fptr;
	}

	switch (state->ssi_state) {
	case SSI_START: /* can never happen: quash a warning */
	case SSI_PATH: {
		file_info_t *ret = fptr->file_symsearch[state->path_index++];

		if (state->path_index >= state->fptr->file_nsymsearch) {
			state->ssi_state = SSI_START_LINEAR;
		}
		return ret;
	}
	case SSI_START_LINEAR:
		state->path_index = 0;
		state->fptr = (file_info_t *) &P->file_list;
		state->ssi_state = SSI_LINEAR;
		/* Fall through */

	case SSI_LINEAR:
		if (state->path_index++ < P->num_files) {
			state->fptr = dt_list_next(state->fptr);
		} else {
			state->ssi_state = SSI_END;
			state->fptr = NULL;
		}
		return state->fptr;

	case SSI_END:
		state->ssi_state = SSI_START;
		state->path_index = 0;
		return NULL;
	}

	/*
	 * This can never happen.
	 */
	return NULL;
}

/*
 * Search the process symbol tables looking for a symbol whose
 * value to value+size contain the address specified by addr.
 * Return values are:
 *	sym_name_buffer  buffer containing the symbol name
 *	GElf_Sym         symbol table entry
 * Returns 0 on success, -1 on failure.
 */
int
Plookup_by_addr(struct ps_prochandle *P, uintptr_t addr, char *sym_name_buffer,
    size_t bufsize, GElf_Sym *symbolp)
{
	GElf_Sym	*symp;
	char		*name;
	GElf_Sym	sym1, *sym1p = NULL;
	GElf_Sym	sym2, *sym2p = NULL;
	char		*name1 = NULL;
	char		*name2 = NULL;
	uint_t		i1;
	uint_t		i2;
	map_info_t	*mptr;
	file_info_t	*fptr;

	if (P->state == PS_DEAD)
		return (-1);

	Pupdate_maps(P);
	Pupdate_lmids(P);

	if ((mptr = Paddr2mptr(P, addr)) == NULL)	/* no such address */
		return (-1);

	fptr = mptr->map_file;

	Pbuild_file_symtab(P, fptr);

	if (fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	/*
	 * Adjust the address by the load object base address in case the
	 * address turns out to be in a shared library.  (This will likely fail
	 * or work only erratically for noninvasive grabs, since we cannot
	 * determine the runtime value of file_dyn_base in that case.)
	 */
	addr -= fptr->file_dyn_base;

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if ((sym1p = sym_by_addr(&fptr->file_symtab, addr, &sym1, &i1)) != NULL)
		name1 = fptr->file_symtab.sym_strs + sym1.st_name;
	if ((sym2p = sym_by_addr(&fptr->file_dynsym, addr, &sym2, &i2)) != NULL)
		name2 = fptr->file_dynsym.sym_strs + sym2.st_name;

	if ((symp = sym_prefer(sym1p, name1, sym2p, name2)) == NULL)
		return (-1);

	name = (symp == sym1p) ? name1 : name2;
	if (bufsize > 0) {
		(void) strncpy(sym_name_buffer, name, bufsize);
		sym_name_buffer[bufsize - 1] = '\0';
	}

	*symbolp = *symp;

	if (GELF_ST_TYPE(symbolp->st_info) != STT_TLS)
		symbolp->st_value += fptr->file_dyn_base;

	return (0);
}
/*
 * Search a specific symbol table looking for a symbol whose name matches the
 * specified name and whose object and link map optionally match the specified
 * parameters.  On success, the function returns 0 and fills in the GElf_Sym
 * symbol table entry, optionally without applying any form of compensation for
 * the load object's load address.  On failure, -1 is returned.
 */
static int
Pxlookup_by_name_internal(
	struct ps_prochandle *P,
	Lmid_t lmid,			/* link map to match, or -1 for any */
	const char *oname,		/* load object name, PR_OBJ_EVERY, or
					   PR_OBJ_LDSO */
	const char *sname,		/* symbol name */
	int fixup_load_addr,		/* compensate for load address */
	GElf_Sym *symp,			/* returned symbol table entry */
	prsyminfo_t *sip)		/* returned symbol info */
{
	file_info_t *fptr;
	int just_one = 0;
	sym_search_iter_t state = {0};

	GElf_Sym sym;
	prsyminfo_t si;
	int rv = -1;
	uint_t id;

	if (P->state == PS_DEAD)
		return (-1);

	memset(&sym, 0, sizeof (GElf_Sym));
	memset(&si, 0, sizeof (prsyminfo_t));

	if (oname == PR_OBJ_EVERY) {
		/* create all the file_info_t's for all the mappings */
		Pupdate_maps(P);
		Pupdate_lmids(P);

		if (!P->info_valid)
			return (-1);

		/*
		 * Start from the executable mapping, if known.
		 */
		if ((P->map_exec != -1) &&
		    (P->mappings[P->map_exec].map_file != NULL))
			fptr = P->mappings[P->map_exec].map_file;
		else
			fptr = dt_list_next(&P->file_list);

		Pupdate_symsearch(P, fptr);
	} else {
		map_info_t *mptr;

		just_one = 1;
		if ((mptr = object_name_to_map(P, lmid, oname)) == NULL)
			return (-1);

		fptr = mptr->map_file;

		Pbuild_file_symtab(P, fptr);
	}

	/*
	 * Iterate through the loaded object files and look for the symbol
	 * name in the .symtab and .dynsym of each.  If we encounter a match
	 * with SHN_UNDEF, keep looking in hopes of finding a better match.
	 * This means that a name such as "puts" will match the puts function
	 * in libc instead of matching the puts PLT entry in the a.out file.
	 */
	while ((fptr = sym_search_next(P, fptr, &state, just_one)) != NULL) {
		Pbuild_file_symtab(P, fptr);

		if (fptr->file_elf == NULL)
			continue;

		if (lmid != PR_LMID_EVERY && fptr->file_lo != NULL &&
		    lmid != fptr->file_lo->rl_lmident)
			continue;

		if (fptr->file_symtab.sym_data_pri != NULL &&
		    sym_by_name(&fptr->file_symtab, sname, symp, &id)) {
			if (sip != NULL) {
				sip->prs_id = id;
				sip->prs_table = PR_SYMTAB;
				sip->prs_object = oname;
				sip->prs_name = sname;
				sip->prs_lmid = fptr->file_lo == NULL ?
				    LM_ID_BASE : fptr->file_lo->rl_lmident;
			}
		} else if (fptr->file_dynsym.sym_data_pri != NULL &&
		    sym_by_name(&fptr->file_dynsym, sname, symp, &id)) {
			if (sip != NULL) {
				sip->prs_id = id;
				sip->prs_table = PR_DYNSYM;
				sip->prs_object = oname;
				sip->prs_name = sname;
				sip->prs_lmid = fptr->file_lo == NULL ?
				    LM_ID_BASE : fptr->file_lo->rl_lmident;
			}
		} else
			continue;

		if (fixup_load_addr && GELF_ST_TYPE(symp->st_info) != STT_TLS)
			symp->st_value += fptr->file_dyn_base;

		if (sym.st_shndx != SHN_UNDEF)
			return (0);

		if (rv != 0 ||
		    sym.st_shndx == SHN_UNDEF) {
			if (sip != NULL)
				si = *sip;
			sym = *symp;
			rv = 0;
		}
	}

	if (rv == 0) {
		if (sip != NULL)
			*sip = si;
		*symp = sym;
	}

	return (rv);
}

/*
 * Search the process symbol tables looking for a symbol whose name matches the
 * specified name and whose object and link map optionally match the specified
 * parameters.  On success, the function returns 0 and fills in the GElf_Sym
 * symbol table entry.  On failure, -1 is returned.
 */
int
Pxlookup_by_name(
	struct ps_prochandle *P,
	Lmid_t lmid,			/* link map to match, or -1 for any */
	const char *oname,		/* load object name, PR_OBJ_EVERY, or
					   PR_OBJ_LDSO */
	const char *sname,		/* symbol name */
	GElf_Sym *symp,			/* returned symbol table entry */
	prsyminfo_t *sip)		/* returned symbol info */
{
	return Pxlookup_by_name_internal (P, lmid, oname, sname, TRUE,
	    symp, sip);
}

/*
 * Iterate over the text mappings of the process's mapped objects.
 */
int
Pobject_iter(struct ps_prochandle *P, proc_map_f *func, void *cd)
{
	map_info_t *mptr;
	file_info_t *fptr;
	uint_t cnt;
	int rc = 0;

	if (P->state == PS_DEAD)
		return (-1);

	Pupdate_maps(P);
	Pupdate_lmids(P);

	if (!P->info_valid)
	    return (-1);

	for (cnt = P->num_files, fptr = dt_list_next(&P->file_list);
	    cnt; cnt--, fptr = dt_list_next(fptr)) {
		const char *lname;

		if (fptr->file_lname != NULL)
			lname = fptr->file_lname;
		else
			lname = "";

		if (fptr->file_map == -1)
			continue;

		mptr = &P->mappings[fptr->file_map];
		if ((rc = func(cd, mptr->map_pmap, lname)) != 0)
			return (rc);

		if (!P->info_valid) {
			Pupdate_maps(P);
			Pupdate_lmids(P);
		}
	}
	return (0);
}

/*
 * Given a virtual address, return the name of the underlying
 * mapped object (file) as provided by the dynamic linker.
 * Failing that, return the basename of its name on disk.
 *
 * Return NULL if we can't find any name information for the object.
 */
char *
Pobjname(struct ps_prochandle *P, uintptr_t addr,
	char *buffer, size_t bufsize)
{
	map_info_t *mptr;
	file_info_t *fptr;

	if (P->state == PS_DEAD)
		return (NULL);

	Pupdate_maps(P);
	Pupdate_lmids(P);

	if ((mptr = Paddr2mptr(P, addr)) == NULL)
		return (NULL);

	if ((fptr = mptr->map_file) == NULL)
		return (NULL);

	if (fptr->file_lname != NULL)
		(void) strlcpy(buffer, fptr->file_lname, bufsize);
	else if (fptr->file_pname != NULL)
		(void) strlcpy(buffer, fptr->file_pname, bufsize);
	else
		return (NULL);

	return (buffer);
}

/*
 * Given a virtual address, return the link map id of the underlying mapped
 * object (file), as provided by the dynamic linker.  Return -1 on failure.
 */
int
Plmid(struct ps_prochandle *P, uintptr_t addr, Lmid_t *lmidp)
{
	map_info_t *mptr;
	file_info_t *fptr;

	if (P->state == PS_DEAD)
		return (-1);

	/* create all the file_info_t's for all the mappings */
	Pupdate_maps(P);
	Pupdate_lmids(P);

	if ((mptr = Paddr2mptr(P, addr)) != NULL &&
	    (fptr = mptr->map_file) != NULL && fptr->file_lo != NULL) {
		*lmidp = fptr->file_lo->rl_lmident;
		return (0);
	}

	return (-1);
}

/*
 * Given an object name, iterate over the object's symbols.
 * If which == PR_SYMTAB, search the normal symbol table.
 * If which == PR_DYNSYM, search the dynamic symbol table.
 */
int
Psymbol_iter_by_addr(struct ps_prochandle *P,
    const char *object_name, int which, int mask, proc_sym_f *func, void *cd)
{
#if STT_NUM != (STT_TLS + 1)
#error "STT_NUM has grown. update Psymbol_iter_com()"
#endif

	GElf_Sym sym;
	GElf_Shdr shdr;
	map_info_t *mptr;
	file_info_t *fptr;
	sym_tbl_t *symtab;
	size_t count;
	const char *strs;
	size_t strsz;
	int rv;
	uint_t *map, i, ndx;

	if (P->state == PS_DEAD)
		return (-1);

	if ((mptr = object_name_to_map(P, PR_LMID_EVERY, object_name)) == NULL)
		return (-1);

	fptr = mptr->map_file;
	Pbuild_file_symtab(P, fptr);

	if (fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	/*
	 * Search the specified symbol table.
	 */
	switch (which) {
	case PR_SYMTAB:
		symtab = &fptr->file_symtab;
		break;
	case PR_DYNSYM:
		symtab = &fptr->file_dynsym;
		break;
	default:
		return (-1);
	}

	strs = symtab->sym_strs;
	strsz = symtab->sym_strsz;
	map = symtab->sym_byaddr;
	count = symtab->sym_count;

	if (symtab->sym_data_pri == NULL || strs == NULL || count == 0)
		return (-1);

	rv = 0;

	for (i = 0; i < count; i++) {
		ndx = map == NULL ? i : map[i];
		if (symtab_getsym(symtab, ndx, &sym) != NULL) {
			uint_t s_bind, s_type, type;
			const char *prs_name;

			if (sym.st_name >= strsz)	/* invalid st_name */
				continue;

			s_bind = GELF_ST_BIND(sym.st_info);
			s_type = GELF_ST_TYPE(sym.st_info);

			/*
			 * In case you haven't already guessed, this relies on
			 * the bitmask used in <libproc.h> for encoding symbol
			 * type and binding matching the order of STB and STT
			 * constants in <sys/elf.h>.  Changes to ELF must
			 * maintain binary compatibility, so I think this is
			 * reasonably fair game.
			 */
			if (s_bind < STB_NUM && s_type < STT_NUM) {
				type = (1 << (s_type + 8)) | (1 << s_bind);
				if ((type & ~mask) != 0)
					continue;
			} else
				continue; /* Invalid type or binding */

			if (GELF_ST_TYPE(sym.st_info) != STT_TLS)
				sym.st_value += fptr->file_dyn_base;

			prs_name = strs + sym.st_name;

			/*
			 * If symbol's type is STT_SECTION, then try to lookup
			 * the name of the corresponding section.
			 */
			if (GELF_ST_TYPE(sym.st_info) == STT_SECTION &&
			    fptr->file_shstrs != NULL &&
			    gelf_getshdr(elf_getscn(fptr->file_elf,
			    sym.st_shndx), &shdr) != NULL &&
			    shdr.sh_name != 0 &&
			    shdr.sh_name < fptr->file_shstrsz)
				prs_name = fptr->file_shstrs + shdr.sh_name;

			if ((rv = func(cd, &sym, prs_name)) != 0)
				break;
		}
	}

	return (rv);
}

/*
 * Return 1 if this address is within a valid mapping.
 */
int
Pvalid_mapping(struct ps_prochandle *P, uintptr_t addr)
{
	if (P->state == PS_DEAD)
		return (-1);

	Pupdate_maps(P);
	Pupdate_lmids(P);
	return (Paddr2mptr(P, addr) != NULL);
}

/*
 * Return 1 if this address is within a file-backed mapping.
 */
int
Pfile_mapping(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;

	if (P->state == PS_DEAD)
		return (-1);

	Pupdate_maps(P);
	Pupdate_lmids(P);
	if ((mptr = Paddr2mptr(P, addr)) != NULL)
		return (mptr->map_file != NULL);
	return 0;
}
/*
 * Return 1 if this address is within a writable mapping.
 */
int
Pwritable_mapping(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;

	if (P->state == PS_DEAD)
		return (-1);

	Pupdate_maps(P);
	Pupdate_lmids(P);
	if ((mptr = Paddr2mptr(P, addr)) == NULL)
		return 0;
	return ((mptr->map_pmap->pr_mflags & MA_WRITE) != 0);
}

/*
 * Called to destroy the symbol tables.  They will be recreated later as needed.
 *
 * Must be called by the client after an exec() in the victim process.
 */
void
Preset_maps(struct ps_prochandle *P)
{
	mapping_purge(P);
	free(P->mappings);
	P->mappings = NULL;

	file_info_purge(P);

	P->info_valid = 0;
}
