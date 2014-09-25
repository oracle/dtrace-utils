/*
 * rtld debugging library.
 */

/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2013, 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <elf.h>
#include <link.h>
#include <pthread.h>
#include <signal.h>
#include <rtld_db.h>
#include <rtld_offsets.h>

#include "libproc.h"
#include "Pcontrol.h"

#define NANO 1000000000LL

#define LOCAL_MAPS_TIMEOUT 7 /* seconds */

/*
 * Read FIELD from STRUCTURE into BUF.  STRUCTURE is rooted at address ADDR in
 * process P; its offsets array is OFFSETS.
 */
#define read_scalar_child(P, buf, addr, offsets, structure, field)	\
	Pread_scalar(P, buf, offsets[offsetof(struct structure, field)].size[P->elf64], \
	    sizeof(((struct structure *)0)->field),			\
	    addr + offsets[offsetof(struct structure, field)].offset[P->elf64])

/*
 * Tripped when ld.so hits our breakpoint on r_brk (used for consistency
 * checking and rtld event reporting).
 */
static int rd_brk_trap(uintptr_t addr, void *rd_data);

/*
 * Tripped when the child process hits our breakpoint on the start address.
 */
static void rd_start_trap(uintptr_t addr, void *rd_data);

/*
 * Try to get the address of r_brk, if not already known: return that address,
 * or 0 if none.
 */
static uintptr_t r_brk(rd_agent_t *rd);

/*
 * Try to get the address of _rtld_global, if not already known: return that
 * address, or 0 if none.
 */
static uintptr_t rtld_global(rd_agent_t *rd);

/*
 * Get a link map, given an address.
 */
static struct link_map *rd_get_link_map(rd_agent_t *rd, struct link_map *buf,
    uintptr_t addr);

/*
 * Wait for the dynamic linker to be in a consistent state.
 */
static int rd_ldso_consistent_begin(rd_agent_t *rd);
static void rd_ldso_consistent_end(rd_agent_t *rd);

/* Internal functions. */

static void
sane_nanosleep(long long timeout_nsec)
{
	struct timespec timeout;

	timeout.tv_sec = timeout_nsec / NANO;
	timeout.tv_nsec = timeout_nsec % NANO;
	nanosleep(&timeout, NULL);
}

/*
 * Determine the number of currently-valid namespaces.
 */
static size_t
dl_nns(rd_agent_t *rd)
{
	size_t buf;

	/*
	 * Non-shared processes always have one and only one namespace, as do
	 * processes for which we have timed out attempting to get at a
	 * consistent state for lmids >1, and those for which we can't get at
	 * their internal state (e.g. those using C libraries other than glibc).
	 */

	if ((rd->P->no_dyn) || (rd->lmid_incompatible_glibc) ||
	    rtld_global(rd) == 0)
		return 1;

	/*
	 * Because this has no corresponding publically-visible header, we must
	 * use offsets directly.  If the read fails, assume 1 (almost always
	 * true anyway).
	 */
	if (Pread_scalar(rd->P, &buf, rd->P->elf64 ? G_DL_NNS_64_SIZE :
		G_DL_NNS_32_SIZE, sizeof (size_t), rtld_global(rd) +
		(rd->P->elf64 ? G_DL_NNS_64_OFFSET : G_DL_NNS_32_OFFSET)) < 0) {
		_dprintf("%i: Cannot read namespace count\n", rd->P->pid);
		return 1;
	}

	return buf;
}

/*
 * Try to get the address of _rtld_global, if not already known: return that
 * address, or 0 if none.
 *
 * Before the dynamic linker is initialized -- i.e., before r_brk is known --
 * this value is invalid, and this function will always return 0.  It is
 * not meaningful in statically-linked programs (and not needed, since we only
 * need it for multiple namespace support, and statically linked programs
 * support only one namespace).
 */
static uintptr_t
rtld_global(rd_agent_t *rd)
{
	GElf_Sym sym = {0};

	if (r_brk(rd) == 0)
	    return 0;

	if (rd->rtld_global_addr)
		return rd->rtld_global_addr;

	/*
	 * Note: Pxlookup_by_name() can eventually call back into rtld_db,
	 * because it calls Pupdate_lmids() which can then call rd_new() and
	 * rd_loadobj_iter().  rd_new() cannot be called because we cannot get
	 * to this point without an rd, but rd_loadobj_iter() can be called if a
	 * mapping update just happened for some reason.  This is still safe
	 * against recursive calls into rtld_global(), because rtld_global() is
	 * only consulted when non-default lmids are looked up, PR_OBJ_LDSO is
	 * always looked up in LM_ID_BASE.
	*/
	if (Pxlookup_by_name(rd->P, 0, PR_OBJ_LDSO,
		"_rtld_global", &sym, NULL) < 0) {
		_dprintf("%i: cannot find _rtld_global.\n", rd->P->pid);
		return 0;
	}

	rd->rtld_global_addr = sym.st_value;

	return rd->rtld_global_addr;
}

/*
 * Determine the address of the Nth r_debug structure, or 0 if unknown or out of
 * bounds.  You only need to use this rather than r_debug() if referencing
 * something which can differ between lmids (so, no need to use it to get at
 * r_brk).
 *
 * This is pure pointer arithmetic, and is very fast.
 */
static uintptr_t
ns_debug_addr(rd_agent_t *rd, Lmid_t lmid)
{
	uintptr_t global;

	/*
	 * The base lmid's r_debug comes from a different place, which is valid
	 * even for statically-linked processes.
	 */
	if (lmid == LM_ID_BASE || rd->P->no_dyn)
		return r_debug(rd->P);

	global = rtld_global(rd);
	if (!global)
		return 0;

	if (lmid >= dl_nns(rd))
		return 0;

	/*
	 * Because this structure is not visible in the systemwide <link.h>, we
	 * cannot use offsetof tricks, but must resort to raw offset computation.
	 */
	return global + (rd->P->elf64 ? G_DEBUG_64_OFFSET : G_DEBUG_32_OFFSET) +
	    (((rd->P->elf64 ? G_DEBUG_SUBSEQUENT_64_OFFSET :
		    G_DEBUG_SUBSEQUENT_32_OFFSET) -
		(rd->P->elf64 ? G_DEBUG_64_OFFSET : G_DEBUG_32_OFFSET)) *
	    lmid);
}

/*
 * Determine the address of the first link map in a given lmid, or 0 if the lmid
 * is unknown or out of bounds.
 */
static uintptr_t
first_link_map(rd_agent_t *rd, Lmid_t lmid)
{
	uintptr_t global;
	uintptr_t link_map_addr;
	uintptr_t link_map_ptr_addr;

	/*
	 * If this is the base lmid, we should get it from r_debug->r_map,
	 * to avoid difficulties finding rtld_global in statically linked
	 * processes.
	 */

	if (lmid == LM_ID_BASE) {
		if (read_scalar_child(rd->P, &link_map_addr,
			ns_debug_addr(rd, lmid), r_debug_offsets, r_debug,
			r_map) < 0)
			return 0;
		return link_map_addr;
	}

	global = rtld_global(rd);
	if (!global)
		return 0;

	if (lmid >= dl_nns(rd))
		return 0;

	/*
	 * Fish the link map straight out of _ns_loaded.
	 */

	link_map_ptr_addr = global + (rd->P->elf64 ? G_NS_LOADED_64_OFFSET :
	    G_NS_LOADED_32_OFFSET) +
	    (((rd->P->elf64 ? G_NS_LOADED_SUBSEQUENT_64_OFFSET :
		    G_NS_LOADED_SUBSEQUENT_32_OFFSET) -
		(rd->P->elf64 ? G_NS_LOADED_64_OFFSET : G_NS_LOADED_32_OFFSET)) *
		lmid);

	if (Pread_scalar(rd->P, &link_map_addr, rd->P->elf64 ? G_NS_LOADED_64_SIZE :
		G_NS_LOADED_32_SIZE, sizeof (struct link_map *), link_map_ptr_addr) < 0) {
		_dprintf("%i: Cannot read link map pointer\n", rd->P->pid);
		return 0;
	}
	return link_map_addr;
}

/*
 * A qsort() / bsearch() function that sorts uintptr_t's into ascending order.
 */
static int
ascending_uintptrs(const void *onep, const void *twop)
{
	uintptr_t one = *(uintptr_t *)onep;
	uintptr_t two = *(uintptr_t *)twop;

	if (one < two)
		return -1;
	else if (two < one)
		return 1;
	else
		return 0;
}

/*
 * Find the offset of the l_searchlist in the link_map structure.
 *
 * Although this offset is relatively constant, it can vary e.g. across glibc
 * upgrades: so carry it out afresh for each process attached to.
 *
 * Returns -1 if nothing resembling a scope searchlist can be found. (May return
 * -1 spuriously in obscure cases, such as processes with no dynamic linker
 * initialized yet, as well as if an exec() strikes while scanning.  In such
 * cases, it will leave the searchlist uninitialized and recheck on every
 * ustack() et al: potentially slow, but the only safe approach.)
 *
 * Must be called under rd_ldso_consistent_begin() to prevent mutation of the
 * set of link maps while we are using them.
 */
static int
find_l_searchlist(rd_agent_t *rd)
{
	/*
	 * We search in the primary link mpa, because we know this must exist if
	 * any do, and that all the link maps will use the same offset for
	 * l_seachlist.
	 *
	 * Iterate through the link maps twice: once to count the number of
	 * maps, the second time to remember their addresses.
	 */
	size_t nmaps = 0;
	uintptr_t first_loadobj, loadobj;
	struct link_map map;

	_dprintf("%i: Finding l_searchlist\n", rd->P->pid);

	if ((first_loadobj = first_link_map(rd, 0)) == 0)
		return -1;

	for (loadobj = first_loadobj; loadobj != 0;
	     loadobj = (uintptr_t) map.l_next) {
		nmaps++;
		if (rd_get_link_map(rd, &map, loadobj) == NULL)
			break;
	}

	_dprintf("%i: Counted %zi link maps\n", rd->P->pid, nmaps);
	if (nmaps == 0)
		return -1;

	/*
	 * After this point, we must not call anything which, even transitively,
	 * calls Pwait(), since that may longjmp out if an execve() is detected,
	 * and leak map_addrs.
	 */

	uintptr_t *map_addrs;
	uintptr_t *mapp;

	map_addrs = calloc(nmaps, sizeof (uintptr_t));
	if (!map_addrs) {
		fprintf(stderr, "Out of memory locating glibc searchlist "
		    "when allocating room for %li link maps\n", nmaps);
		exit(1);
	}

	for (mapp = map_addrs,
		 loadobj = first_link_map(rd, 0);
	     loadobj != 0;
	     loadobj = (uintptr_t) map.l_next, mapp++) {

		_dprintf("%i: Noted map at %lx\n", rd->P->pid, loadobj);

		*mapp = loadobj;
		if (rd_get_link_map(rd, &map, loadobj) == NULL)
			break;
	}

	qsort(map_addrs, nmaps, sizeof (uintptr_t), ascending_uintptrs);

	/*
	 * We now have the primary link maps' addresses in a rapidly searchable
	 * form.  Search forward in the first link map from the offset of
	 * l_searchlist in glibc 2.12 (an optimization, since it is implausible
	 * that its offset will ever fall), until we fall off the end of
	 * accessible memory, or until we find a pointer followed by an integer,
	 * where the integer is at least 2 and the pointer points to an array of
	 * that many addresses in map_addrs.  (For sanity, cap the iteration at
	 * 64K, so as not to waste too much time in a massive search which can
	 * never succeed.)
	 *
	 * If we find such an item, this is l_searchlist.  (There are other
	 * r_scope_elems in the link_map structure, but they are all *pointers*
	 * to such, and there are no other link map arrays followed by a count.
	 * If a new one is ever inserted before l_searchlist but after its glibc
	 * 2.12 offset, this algorithm will break, but for now we are trying to
	 * compensate for shifts in offset caused by addition of things like new
	 * ELF dynamic tags, expanding the l_info array, which is not an array
	 * of pointers to link maps, thus will never cause this algorithm to
	 * fail.)
	 *
	 * Do all the reads in this process quietly: we may be probing a lot of
	 * things that aren't addresses.
	 *
	 * As for the size of the hop to make: we know the address of the
	 * pointer to the link map, since this is an array, but have no explicit
	 * reference to its size.  However, we have other things which we know
	 * must always be pointers to link maps, like L_NEXT.  (It also happens
	 * that POSIX implicitly requires pointers to void and to functions to
	 * be interconvertible, but that seems like a bad thing to rely on when
	 * we don't have to, even here.)
	 */

	uintptr_t scan;
	uintptr_t scan_next;

	scan = first_loadobj;
	scan += rd->P->elf64 ? L_SEARCHLIST_64_OFFSET : L_SEARCHLIST_32_OFFSET;
	scan_next = scan + (rd->P->elf64 ? L_NEXT_64_SIZE : L_NEXT_32_SIZE);

	for (;; scan += (rd->P->elf64 ? UINT_64_SIZE : UINT_32_SIZE),
		scan_next += (rd->P->elf64 ? UINT_64_SIZE : UINT_32_SIZE)) {

		uintptr_t poss_l_searchlist_r_list;
		unsigned int poss_l_searchlist_r_nlist;

		_dprintf("%i: scanning from link_map offset %lx\n", rd->P->pid,
		    scan - first_loadobj);

		if ((scan - first_loadobj) > 65535)
			break;

		if (Pread_scalar_quietly(rd->P, &poss_l_searchlist_r_list,
			rd->P->elf64 ? L_NEXT_64_SIZE : L_NEXT_32_SIZE,
			sizeof (uintptr_t), scan, 1) < 0) {
			break;
		}

		if (Pread_scalar_quietly(rd->P, &poss_l_searchlist_r_nlist,
			rd->P->elf64 ? UINT_64_SIZE : UINT_32_SIZE,
			sizeof (unsigned int), scan_next, 1) < 0) {
			break;
		}

		/*
		 * A possible scope array.  Is it long enough?
		 */
		if (poss_l_searchlist_r_nlist < 2)
			continue;

		uintptr_t scope_array_scan;
		unsigned int nscopes;
		int unmatched = 0;

		/*
		 * Now scan the possible scopes list until we hit the end or
		 * find something that isn't in map_addrs.
		 */

		scope_array_scan = poss_l_searchlist_r_list;

		for (nscopes = 0; nscopes < poss_l_searchlist_r_nlist;
		     nscopes++,
		     scope_array_scan += rd->P->elf64 ?
			 L_NEXT_64_SIZE : L_NEXT_32_SIZE)
		{
			uintptr_t poss_map;

			if (Pread_scalar_quietly(rd->P, &poss_map,
				rd->P->elf64 ? L_NEXT_64_SIZE : L_NEXT_32_SIZE,
				sizeof (uintptr_t), scope_array_scan, 1) < 0) {
				unmatched = 1;
				break;
			}

			if (bsearch(&poss_map, map_addrs, nmaps,
				sizeof (uintptr_t),
				ascending_uintptrs) == NULL) {
				unmatched = 1;
				break;
			}
			_dprintf("%i: %i: matched with %lx\n", rd->P->pid,
			    nscopes, poss_map);
		}

		/*
		 * If everything matched, this is the l_searchlist.
		 */

		if (!unmatched) {
			free(map_addrs);
			rd->l_searchlist_offset = scan - first_loadobj;
			_dprintf("%i: found l_searchlist at offset %zi\n",
			    rd->P->pid, rd->l_searchlist_offset);
			return 0;
		}
	}

	free(map_addrs);
	_dprintf("%i: no searchlist found\n", rd->P->pid);
	return -1;
}

/*
 * Get a link map, given an address.
 */
static struct link_map *
rd_get_link_map(rd_agent_t *rd, struct link_map *buf, uintptr_t addr)
{
	if ((read_scalar_child(rd->P, &buf->l_addr, addr,
		    link_map_offsets, link_map, l_addr) <= 0) ||
	    (read_scalar_child(rd->P, &buf->l_name, addr,
		link_map_offsets, link_map, l_name) <= 0) ||
	    (read_scalar_child(rd->P, &buf->l_ld, addr,
		link_map_offsets, link_map, l_ld) <= 0) ||
	    (read_scalar_child(rd->P, &buf->l_next, addr,
		link_map_offsets, link_map, l_next) <= 0) ||
	    (read_scalar_child(rd->P, &buf->l_prev, addr,
		link_map_offsets, link_map, l_prev) <= 0))
		return NULL;

	return buf;
}

/*
 * Populate a loadobj element, given a link map and an address.
 *
 * The rl_scope array in the returned structure is realloc()ed as needed, should
 * be NULL to start with, and must be freed by the caller when done.
 *
 * rl_lmident is not populated.
 */
static struct rd_loadobj *
rd_get_loadobj_link_map(rd_agent_t *rd, rd_loadobj_t *buf,
    struct link_map *map, uintptr_t addr)
{
	uintptr_t searchlist;
	size_t i;

	if (rd_ldso_consistent_begin(rd) != 0)
		return NULL;

	if (!rd->l_searchlist_offset)
		if (find_l_searchlist(rd) < 0)
			goto fail;

	buf->rl_diff_addr = map->l_addr;
	buf->rl_nameaddr = (uintptr_t) map->l_name;
	buf->rl_dyn = (uintptr_t) map->l_ld;

	/*
	 * Now put together the scopes array.  Avoid calling the allocator too
	 * often by allocating the array only once, expanding it as needed.
	 *
	 * We are grabbing a pointer to a link map.  We know its address, since
	 * this is an array, but have no explicit reference to its
	 * size. However, we have other things which we know must always be
	 * pointers to link maps -- like L_NEXT.
	 */

	if (Pread_scalar(rd->P, &searchlist, rd->P->elf64 ?
		L_NEXT_64_SIZE : L_NEXT_32_SIZE,
		sizeof (uintptr_t), addr + rd->l_searchlist_offset) < 0)
		goto fail;

	if (Pread_scalar(rd->P, &buf->rl_nscopes, rd->P->elf64 ?
		UINT_64_SIZE : UINT_32_SIZE,
		sizeof (unsigned int), addr + rd->l_searchlist_offset +
		(rd->P->elf64 ? L_NEXT_64_SIZE : L_NEXT_32_SIZE)) < 0)
		goto fail;

	if (buf->rl_nscopes_alloced < buf->rl_nscopes) {
		size_t nscopes_size = buf->rl_nscopes * sizeof(uintptr_t);

		buf->rl_scope = realloc(buf->rl_scope, nscopes_size);
		if (buf->rl_scope == NULL) {
			fprintf(stderr, "Out of memory allocating scopes list "
			    "of length %lu bytes\n", nscopes_size);
			exit(1);
		}

		buf->rl_nscopes_alloced = buf->rl_nscopes;
	}

	for (i = 0; i < buf->rl_nscopes; i++) {
		size_t link_map_ptr_size = rd->P->elf64 ? L_NEXT_64_SIZE :
		    L_NEXT_32_SIZE;

		if (Pread_scalar(rd->P, &buf->rl_scope[i], link_map_ptr_size,
			sizeof (uintptr_t), searchlist +
			(i * link_map_ptr_size)) < 0)
			goto fail;
	}

	rd_ldso_consistent_end(rd);
	return buf;
fail:
	rd_ldso_consistent_end(rd);
	return NULL;
}

/*
 * Determine the load count of namespace N, where N > 0.
 */
static unsigned int
ns_nloaded(rd_agent_t *rd, Lmid_t lmid)
{
	unsigned int buf;
	uintptr_t addr;

	addr = rtld_global(rd) +
	    (rd->P->elf64 ? G_NLOADED_64_OFFSET : G_NLOADED_32_OFFSET) +
	    (((rd->P->elf64 ? G_NLOADED_SUBSEQUENT_64_OFFSET :
		    G_NLOADED_SUBSEQUENT_32_OFFSET) -
		(rd->P->elf64 ? G_NLOADED_64_OFFSET : G_NLOADED_32_OFFSET)) *
		lmid);

	/*
	 * If the read fails, assume 1 (almost always true anyway).
	 */
	if (Pread_scalar(rd->P, &buf, rd->P->elf64 ? G_NLOADED_64_SIZE :
		G_NLOADED_32_SIZE, sizeof (unsigned int), addr) < 0) {
		_dprintf("%i: Cannot read loaded object count\n", rd->P->pid);
		return 1;
	}

	return buf;
}

/*
 * Get the value of the _dl_load_lock, which is nonzero whenever a dlopen() or
 * dlclose() is underway.
 */
static unsigned int
load_lock(rd_agent_t *rd)
{
	unsigned int lock_count;

	/*
	 * This should never happen: if it does, let's not read garbage.
	 */

	if (rtld_global(rd) == 0)
		return 0;

	if (Pread_scalar(rd->P, &lock_count, rd->P->elf64 ?
		G_DL_LOAD_LOCK_64_SIZE : G_DL_LOAD_LOCK_32_SIZE,
		sizeof (unsigned int), rtld_global(rd) +
		(rd->P->elf64 ? G_DL_LOAD_LOCK_64_OFFSET :
		    G_DL_LOAD_LOCK_32_OFFSET)) < 0)
		return -1;

	return lock_count;
}

/*
 * Dynamic linker consistency enforcement.
 *
 * The dynamic linker's link maps are consistent only at particular
 * points.
 *
 * dlopen()/dlclose()
 * .
 * .
 * _rtld_global._dl_load_lock     A
 * taken out
 * .
 * .
 * consistency = RD_{ADD|DELETE}  B
 * _r_debug._r_brk()              C
 * .
 * .
 * consistency = RD_CONSISTENT    D
 * _r_debug._r_brk()              E
 * .
 * .
 * _rtld_global._dl_load_lock     F
 * released
 * .
 * . dlopen()/dlclose() returns
 *
 * The main link map (identified by _r_debug.r_map) is consistent at all points
 * except between C and D.  So we can enforce consistency by dropping a
 * breakpoint on _r_brk and halting the process iff it is hit with a consistency
 * other than RD_CONSISTENT (in which case it is about to go inconsistent), and
 * ensure initial consistency by waiting until the consistency flag is
 * RD_CONSISTENT (indicating that we are past point E) *or* we hit _r_brk()
 * (indicating that we are at point C).
 *
 * The secondary link maps (for lmids != 0) are more annoying.  These are only
 * consistent between D..E, and outside the region A..F where the _dl_load_lock
 * is taken out.  So we can only enforce consistency by waiting until either we
 * halt at E or the _dl_load_lock drops to zero, then forcibly stopping the
 * process until we no longer need consistency because we can't otherwise
 * guarantee that control won't flow back into point A again.  Because this lock
 * is in another process and is maintained by a private futex, we can't block on
 * it, so we busywait with exponential backoff instead.
 */

/*
 * Read dynamic linker consistency information out of the traced child.
 *
 * An lmid of -1 means to read all lmids and return the 'most inconsistent'
 * one.  If more than one lmid is in inconsistent state, it is undefined
 * whether RD_ADD or RD_DELETE is returned.
 *
 * Do not call for a specific lmid unless you know that lmid exists.
 *
 * (A value !RD_CONSISTENT does not necessarily indicate that we *are*
 * inconsistent, merely that we *may* be: we may be stopped on the r_brk
 * breakpoint, and consistent.  To ensure consistency, use
 * rd_ldso_consistent_begin()/rd_ldso_consistent_end(), which handle this
 * ambiguity.)
 */
static int
rd_ldso_consistency(rd_agent_t *rd, Lmid_t lmid)
{
	int r_state = RD_CONSISTENT;
	Lmid_t i;

	if (lmid > -1) {
		uintptr_t lmid_addr = ns_debug_addr(rd, lmid);

		if (lmid_addr == 0 ||
		    read_scalar_child(rd->P, &r_state, lmid_addr,
			r_debug_offsets, r_debug, r_state) <= 0) {

			static int warned = 0;
			/*
			 * Read failed? Warn, but assume consistent (because we
			 * must, or we will block forever).
			 */
			if (!warned)
				_dprintf("Cannot read r_state of LMID %li to "
				    "determine consistency of child with "
				    "PID %i.\n", lmid, rd->P->pid);
			warned = 1;
			return RD_CONSISTENT;
		}

		if (r_state != RD_CONSISTENT)
			_dprintf("%i: Map for lmid %li is inconsistent\n",
			    rd->P->pid, lmid);

		return r_state;
	}

	for (i = 0; i < dl_nns(rd) && r_state == RD_CONSISTENT;
	     i++)
		r_state = rd_ldso_consistency(rd, i);

	return r_state;
}

/*
 * Wait for the dynamic linker to be in a consistent state.
 *
 * On exit, the process may be running, or it may be stopped: but until
 * rd_ldso_consistent_end() is called, it will be in a consistent state (and may
 * transition into stopped state at any time to enforce this).
 *
 * Pwait()ing on a process between an rd_ldso_consistent_begin() and
 * rd_ldso_consistent_end() may deadlock.
 *
 * Alas, this cannot guarantee that link maps for nonzero lmids are not
 * modified.  See rd_ldso_nonzero_lmid_consistent_begin()/end().
 */
static int
rd_ldso_consistent_begin(rd_agent_t *rd)
{
	int err;

	/*
	 * If we are already inside a consistency-enforcement block, just note
	 * the increased nesting level and return: we are necessarily already
	 * consistent, so there is nothing to do.
	 */
	if (rd->no_inconsistent > 0) {
		rd->no_inconsistent++;
		return 0;
	}

	/*
	 * If we are already stopped at a breakpoint or otherwise in tracing
	 * stop, do nothing.  Return success, unless we are in an inconsistent
	 * state (in which case we cannot move into a consistent state without
	 * causing trouble elsewhere).
	 */
	if ((rd->P->bkpt_halted) || (rd->P->state != PS_RUN)) {
		if (rd_ldso_consistency(rd, LM_ID_BASE) == RD_CONSISTENT)
			return 0;
		else
			return EDEADLK;
	}

	/*
	 * Begin ptracing, and record the run state of the process before
	 * consistency enforcement began.
	 */
	Ptrace(rd->P, 0);
	rd->prev_state = rd->P->state;

	/*
	 * Reset the marker that signals that a state transition was detected,
	 * bar transition of the dynamic linker into an inconsistent state (if
	 * it is currently in a consistent state), and arrange to signal when a
	 * transition is detected. Do all this before we drop the breakpoint to
	 * start monitoring.
	 */
	rd->ic_transitioned = 0;
	rd->no_inconsistent++;

	/*
	 * Set up the breakpoint to detect consistency state transitions.
	 */
	if (!rd->rd_monitoring) {
		err = Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap, NULL, rd);
		rd->rd_monitoring = TRUE;
		if (err != 0) {
			rd->no_inconsistent--;
			return err;
		}
	}

	/*
	 * If we are currently in an inconsistent state, then wait until we
	 * transition into a consistent state, or die.  (If we are not currently
	 * in an inconsistent state, a transition at this point will halt us
	 * immediately before the link maps become inconsistent, though
	 * rd_ldso_consistency() will not report RD_CONSISTENT in this case.)
	 *
	 * If it were possible for a dlopen() on one lmid to happen while a
	 * dlopen() on another was in progress, this would fail: we would block
	 * before the first link map became inconsistent, leaving the second in
	 * an inconsistent state and the process blocked.  Fortunately link map
	 * modifications are protected by the dl_load_lock inside ld.so, so
	 * concurrent modifications are prohibited (and at most one link map can
	 * be in non-RD_CONSISTENT state at any given time).
	 *
	 * This all gets a little more complex if the process is already halted.
	 * Resume it, but tell the breakpoint handler that on a transition to a
	 * consistent state we should halt again.
	 */
	if (rd_ldso_consistency(rd, LM_ID_BASE) != RD_CONSISTENT) {

		_dprintf("%i: link maps inconsistent: waiting for "
		    "transition.\n", rd->P->pid);

		if (rd->P->state == PS_STOP ||
		    rd->P->state == PS_TRACESTOP) {
			Pbkpt_continue(rd->P);
			rd->stop_on_consistent = 1;
		}

		/*
		 * If we die inside the dynamic linker, drop out.  If we
		 * silently become consistent, drop out.  If someone drops a
		 * breakpoint somewhere inside the dynamic linker, we will
		 * return with inconsistent link maps.  Don't do that.
		 */
		Pwait(rd->P, FALSE);
		while (!rd->ic_transitioned && (rd->P->state == PS_RUN ||
			rd->P->group_stopped) &&
		    rd_ldso_consistency(rd, LM_ID_BASE) != RD_CONSISTENT)
			Pwait(rd->P, TRUE);

		rd->stop_on_consistent = 0;
	}

	if (rd->P->state == PS_DEAD)
		return -1;

	return 0;
}

static void
rd_ldso_consistent_end(rd_agent_t *rd)
{
	/*
	 * Protect against unbalanced calls, and calls from inside a breakpoint
	 * handler.
	 */
	if (!rd->no_inconsistent)
		return;

	rd->no_inconsistent--;

	if (rd->no_inconsistent)
		return;

	/*
	 * Turn off the breakpoint, if it's not being used for anything else.
	 */
	if (!rd->rd_event_fun) {
		Punbkpt(rd->P, rd->r_brk_addr);
		rd->rd_monitoring = FALSE;
	}

	/*
	 * If the breakpoint is still there and we are stopped on it, move on.
	 */
	if (Pbkpt_addr(rd->P) != 0)
		Pbkpt_continue(rd->P);
	Puntrace(rd->P, rd->prev_state);
}

/*
 * glibc does not guarantee that the link map for lmids > 0 are consistent:
 * r_brk guards only the link map for lmid 0.  If nonzero lmids are being
 * traversed, these functions can be used to ensure that their link maps are
 * consistent.  They are relatively inefficient (they can stop the process), so
 * use only when necessary.  They must be used within
 * rd_ldso_consistent_begin()/end().
 */
static int
rd_ldso_nonzero_lmid_consistent_begin(rd_agent_t *rd)
{
	long long timeout_nsec;

	/*
	 * If we are already stopped at a breakpoint or otherwise in tracing
	 * stop, do nothing.  Return success, unless we are in an inconsistent
	 * state (in which case we cannot move into a consistent state without
	 * causing trouble elsewhere).
	 */
	if ((rd->P->bkpt_halted) || (rd->P->state != PS_RUN)) {
		if (rd_ldso_consistency(rd, -1) == RD_CONSISTENT)
			return 0;
		else
			return EDEADLK;
	}

	/*
	 * If we have detected that our idea of the location of ld.so's load
	 * lock is probably wrong, abort immediately.
	 */
	if (rd->lmid_incompatible_glibc)
		return -1;

	/*
	 * Note that we want to halt on transition to a *consistent* state now,
	 * not an inconsistent one.  (After this Pwait(), we may be halted
	 * at either state, due to earlier Pwait()s.)
	 */
	rd->stop_on_consistent = 1;

	Pwait(rd->P, FALSE);

	if (rd->P->state == PS_DEAD)
		return -1;

	/*
	 * If we are halted at the breakpoint with a consistent link map we're
	 * in an acceptable state.
	 *
	 * Also, a sanity check: in this situation, the load lock must be
	 * nonzero.
	 */
	if (rd->ic_transitioned && rd_ldso_consistency(rd, -1) == RD_CONSISTENT
	    && rd->P->state != PS_RUN) {
		if (load_lock(rd) == 0) {
			rd->lmid_incompatible_glibc = 1;
			rd->stop_on_consistent = 0;

			_dprintf("%i: definitely inside dynamic linker, "
			    "but _rtld_global_dl_load_lock appears zero: "
			    "probable glibc internal data structure change\n",
			    rd->P->pid);
			return -1;
		}
		return 0;
	}

	/*
	 * If we are halted at the breakpoint, get going again: after the check
	 * above we know we cannot be RD_CONSISTENT, therefore nonzero link maps
	 * are already in flux.
	 */
	if (rd->ic_transitioned && rd->P->state != PS_RUN) {
		rd->ic_transitioned = 0;
		Pbkpt_continue(rd->P);
	} else {
		unsigned int lock_count;

		/*
		 * Stop the process while we check the state of the load lock.
		 * If it dies before this, fail.
		 */
		rd->lmid_halt_prev_state = Ptrace(rd->P, 1);
		if (rd->lmid_halt_prev_state == PS_DEAD)
			return -1;

		if ((lock_count = load_lock(rd)) < 0) {
			_dprintf("%i: Cannot read load lock count\n",
			    rd->P->pid);
			Puntrace(rd->P, rd->lmid_halt_prev_state);
			return 0;
		}

		/*
		 * Lock not taken out.  Map consistent, as long as we are halted.
		 */
		if (lock_count == 0) {
			rd->lmid_halted = 1;
			return 0;
		}

		rd->ic_transitioned = 0;
		Puntrace(rd->P, rd->lmid_halt_prev_state);

		if (rd->P->state == PS_TRACESTOP)
			Pbkpt_continue(rd->P);
	}

	/*
	 * Lock taken out or halted in inconsistent state.  Maps may be
	 * inconsistent.  Wait until we hit the rd_brk breakpoint or the lock
	 * drops to zero, at which point the maps will have ceased changing.
	 *
	 * This lock is the only nonpublic, non-ABI-guaranteed part of glibc on
	 * which we depend, so add an arbitrary threshold: if enough time passes
	 * and we do not wake up, assume that this is not in fact the lock.  In
	 * this situation, we don't know where the lock is, so we can't tell if
	 * the link maps are changing, and must abort: iteration over lmids > 1
	 * will fail, but that is a minor consequence.
	 */

	/*
	 * Actually loop, waiting for the process to not be in stopped state and
	 * for the breakpoint to be hit.  Note that the lock is taken out before
	 * _dl_debug_state() is called for the first time, so we can hit the
	 * breakpoint more than once.  So wait until we hit the breakpoint at
	 * least once *and* stop running.
	 */

	do {
		Pwait(rd->P, FALSE);
	} while (rd->P->state == PS_TRACESTOP);

	timeout_nsec = 1000000;
	while (rd->P->state == PS_RUN && load_lock(rd) > 0) {
		/*
		 * Timeout.  Do a continue just in case this is a false alarm
		 * and we hit the breakpoint just as we were aborted, then fail.
		 */
		if (timeout_nsec > LOCAL_MAPS_TIMEOUT * NANO) {
			Pbkpt_continue(rd->P);
			rd->lmid_incompatible_glibc = 1;
			rd->stop_on_consistent = 0;

			_dprintf("%i: timeout waiting for r_brk, probable "
			    "_rtld_global_dl_load_lock glibc internal data "
			    "structure change\n", rd->P->pid);

			return -1;
		}

		Pwait(rd->P, FALSE);
		sane_nanosleep(timeout_nsec);
		timeout_nsec *= 2;
	}

	/*
	 * Halt the process, if not already breakpointed to a halt.
	 */
	if (rd->ic_transitioned && rd->P->state == PS_TRACESTOP)
		rd->lmid_bkpted = TRUE;
	else if (rd->P->state == PS_DEAD)
		return -1;
	else {
		rd->lmid_halted = 1;
		rd->lmid_halt_prev_state = Ptrace(rd->P, 1);
	}

	return 0;
}

/*
 * End a nonzero-lmid consistency window, possibly resuming the process if it
 * was halted to prevent inconsistency.
 */
static void rd_ldso_nonzero_lmid_consistent_end(rd_agent_t *rd)
{
	if (rd->lmid_halted)
		Puntrace(rd->P, rd->lmid_halt_prev_state);
	else if (rd->lmid_bkpted)
		Pbkpt_continue(rd->P);

	rd->stop_on_consistent = 0;
	rd->lmid_halted = 0;
	rd->lmid_bkpted = 0;
}

/*
 * Tripped after ld.so hits our breakpoint on r_brk and singlesteps past it
 * (used for consistency checking and rtld event reporting).
 */
static int
rd_brk_trap(uintptr_t addr, void *rd_data)
{
	rd_agent_t *rd = rd_data;
	int consistency = rd_ldso_consistency(rd, -1);
	int ret = PS_RUN;

	rd->ic_transitioned = 1;

	/*
	 * Block if inconsistent state transitions are barred, and we are
	 * transitioning to an inconsistent state (for normal consistency traps,
	 * in which we can run until a dlopen()/dlclose() happens, and then
	 * stop), or to a consistent state (for lmid-nonzero traps).
	 */

	_dprintf("%i: rd_brk breakpoint hit, consistency is %i; %s\n",
	    rd->P->pid, consistency,
	    consistency == RD_CONSISTENT ? "consistent" : "inconsistent");

	if (rd->no_inconsistent) {
	    if (!rd->stop_on_consistent && (consistency != RD_CONSISTENT))
		ret = PS_TRACESTOP;
	    else if (rd->stop_on_consistent && (consistency == RD_CONSISTENT))
		ret = PS_TRACESTOP;
	}

	/*
	 * If we are doing event monitoring, call the callback.
	 */
	if (rd->rd_event_fun) {
		rd_event_msg_t msg;
		msg.type = RD_DLACTIVITY;
		msg.state = consistency;

		_dprintf("%i: rtld activity event fired.\n", rd->P->pid);
		rd->rd_event_fun(rd, &msg, rd->rd_event_data);
	}

	return ret;
}

/*
 * Try to get the address of r_brk, if not already known: return that address,
 * or 0 if none.
 *
 * In dynamically linked programs, we can almost always get this successfully,
 * as long as the dynamic linker has finished initializing: but in statically
 * linked programs, this is lazily initialized when dlopen() is called for the
 * first time.  So we must initialize it later.
 *
 * All entry points to rtld_db which directly or indirectly access r_brk_addr
 * must test that r_brk(rd) != 0 first!
 */
static uintptr_t
r_brk(rd_agent_t *rd)
{
	static int warned = 0;
	uintptr_t r_debug_addr;
	int r_version;

	if (rd->r_brk_addr)
		return rd->r_brk_addr;

	r_debug_addr = r_debug(rd->P);

	/*
	 * An _r_debug address or version of zero means that the dynamic linker
	 * has not yet initialized.  -1 means that there is no _r_debug at all.
	 */
	if ((r_debug_addr == -1) ||
	    (r_debug_addr == 0))
		return 0;

	/*
	 * Check its version.
	 */
	if ((read_scalar_child(rd->P, &r_version, r_debug_addr,
		    r_debug_offsets, r_debug, r_version) <= 0) ||
	    (r_version > 1)) {
		if (!warned)
			_dprintf("%i: r_version %i unsupported.\n",
			    rd->P->pid, r_version);
		warned = 1;
		return 0;
	}

	if (r_version == 0)
		return 0;

	/*
	 * Get the address of the r_brk dynamic linker breakpoint.
	 */
	if ((read_scalar_child(rd->P, &rd->r_brk_addr, r_debug_addr,
		    r_debug_offsets, r_debug, r_brk) <= 0) ||
	    (rd->r_brk_addr == 0)) {
		if (!warned)
			_dprintf("%i: Cannot determine dynamic linker "
			    "breakpoint address.\n", rd->P->pid);
	}

	return rd->r_brk_addr;
}

/*
 * Tripped when we hit our breakpoint on the user entry point.
 *
 * At this point, in a dynamically linked program, the dynamic linker is
 * initialized.
 */
static void
rd_start_trap(uintptr_t addr, void *rd_data)
{
	rd_agent_t *rd = rd_data;
	uintptr_t r_debug_addr = r_debug(rd->P);

	/*
	 * Can't find r_debug, even though ld.so has initialized?  Abandon all
	 * attempts to track dynamic linker state: we will pick things up again
	 * when another exec() happens.
	 */

	if (r_debug_addr == -1) {
		_dprintf("Cannot initialize rd_agent for PID %i: no r_debug.\n",
		    rd->P->pid);
		goto done;
	}

	if (r_debug_addr == 0 || r_brk(rd) == 0) {
		_dprintf("%i: Cannot determine dynamic linker load map address.\n",
		    rd->P->pid);
		goto done;
	}

	if (!rd->P->no_dyn && rtld_global(rd) == 0) {
		_dprintf("%i: Cannot determine dynamic linker global map "
		    "address.\n", rd->P->pid);
		goto done;
	}

	/*
	 * Reactivate the rd_monitoring breakpoint, if it should be active (e.g.
	 * if it was active before an exec()).  Silently disable monitoring if
	 * we cannot reactivate the breakpoint.  Automatically trigger a
	 * dlopen() callback.
	 */
	if (rd->rd_monitoring) {
		if (Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap,
			NULL, rd) != 0)
			rd->rd_monitoring = FALSE;
		else {
			rd_event_msg_t msg;
			msg.type = RD_DLACTIVITY;
			msg.state = RD_CONSISTENT;

			_dprintf("%i: initial rtld activity event fired.\n",
			    rd->P->pid);
			rd->rd_event_fun(rd, &msg, rd->rd_event_data);
		}
	}

	rd->maps_ready = 1;

done:
	_dprintf("%i: Hit start trap, r_brk is %lx; removing breakpoint\n",
	    rd->P->pid, rd->r_brk_addr);
	Punbkpt(rd->P, addr);
}

/* API functions. */ 

/*
 * Initialize a rd_agent.
 */
rd_agent_t *
rd_new(struct ps_prochandle *P)
{
	rd_agent_t *rd;
	int r_version = 0;
	int oldstate;
	uintptr_t r_debug_addr;

	/*
	 * Give up right away if the process is dead.
	 */
	if (P->state == PS_DEAD) {
		_dprintf ("%i: Cannot initialize rd_agent: "
		    "process is dead.\n", P->pid);
		return (NULL);
	}

	/*
	 * Can't find r_debug? This is hopeless. Maybe this is a stripped
	 * statically linked binary, or this is a noninvasive ptrace().
	 */
	r_debug_addr = r_debug(P);
	if (r_debug_addr == -1) {
                _dprintf("%i: Cannot initialize rd_agent: no "
                    "r_debug.\n", P->pid);
		return (NULL);
	}

	Pwait(P, 0);

	rd = calloc(sizeof (struct rd_agent), 1);
	if (rd == NULL)
		return (NULL);
	/*
	 * Protect against multiple calls.
	 */
	if (P->rap) {
		free(rd);
		return (P->rap);
        }

	rd->P = P;
	P->rap = rd;
	oldstate = Ptrace(P, 1);

	/*
	 * Check its version.  An _r_debug address or version of zero means that
	 * the dynamic linker has not yet initialized.
	 */
	if (r_debug_addr != 0 &&
	    ((read_scalar_child(rd->P, &r_version, r_debug_addr,
		    r_debug_offsets, r_debug, r_version) <= 0) ||
		(r_version > 1))) {
		_dprintf("%i: r_version %i unsupported.\n", rd->P->pid,
		    r_version);
		goto err;
	}

	/*
	 * If the dynamic linker is initialized, get the address of the r_brk
	 * dynamic linker breakpoint, and of the rtld_global structure.  (We
	 * cannot trust this otherwise, because it will move around when the
	 * dynamic linker relocates itself.
	 */

	if (r_version > 0) {
		if ((read_scalar_child(rd->P, &rd->r_brk_addr, r_debug_addr,
			    r_debug_offsets, r_debug, r_brk) <= 0) ||
		    (rd->r_brk_addr == 0)) {
			_dprintf("%i: Cannot determine dynamic linker "
			    "breakpoint address.\n", rd->P->pid);
			goto err;
		}
		if (!P->no_dyn && rtld_global(rd) == 0) {
			_dprintf("%i: Cannot determine dynamic linker "
			    "global map address.\n", rd->P->pid);
		}

		rd->maps_ready = 1;
	} else
		/*
		 * Dynamic linker not yet initialized.  Drop a notifier on the
		 * entry address, so we can initialize it then.  If this is a
		 * statically linked binary, this is not sufficient: _r_debug is
		 * initialized at an unknown distant future point, when
		 * the first shared library is loaded.  We'll just return
		 * RD_NOMAPS until then. 
		 */

		if (!P->no_dyn &&
		    Pbkpt_notifier(rd->P, Pgetauxval(rd->P, AT_ENTRY), FALSE,
			rd_start_trap, NULL, rd) != 0) {
			_dprintf("%i: cannot drop breakpoint at process start.\n",
			    rd->P->pid);
			goto err;
		}

	Puntrace(P, oldstate);

	_dprintf("%i: Activated rtld_db agent.\n", rd->P->pid);
	return (rd);
err:
	Puntrace(P, oldstate);
	free(P->rap);
	P->rap = NULL;
	return (NULL);
}

/*
 * Shut down a rd_agent.
 */
void
rd_delete(rd_agent_t *rd)
{
	/*
	 * Protect against NULL calls, and multiple calls.
	 */
	if (!rd)
		return;

	if (rd->P->rap != rd)
		return;

	while (rd->no_inconsistent)
		rd_ldso_consistent_end(rd);

	rd_event_disable(rd);

	if (rd->rd_monitoring) {
		Punbkpt(rd->P, rd->r_brk_addr);
		Punbkpt(rd->P, Pgetauxval(rd->P, AT_ENTRY)); /* just in case */
	}
	rd->P->rap = NULL;

	_dprintf("%i: Deactivated rtld_db agent.\n", rd->P->pid);
	free(rd);
}

/*
 * Enable DLACTIVITY monitoring.
 */
rd_err_e
rd_event_enable(rd_agent_t *rd, rd_event_fun fun, void *data)
{
	rd->rd_event_fun = fun;
	rd->rd_event_data = data;

	if (rd->rd_monitoring)
		return RD_OK;

	if ((r_brk(rd) == 0) || (!rd->maps_ready))
		return RD_NOMAPS;

	if (Pbkpt(rd->P, rd->r_brk_addr, FALSE, rd_brk_trap, NULL, rd) != 0)
		return RD_ERR;

	rd->rd_monitoring = TRUE;
	_dprintf("%i: enabled rtld activity monitoring.\n", rd->P->pid);

	return RD_OK;
}

/*
 * Disable DLACTIVITY monitoring.
 */
void
rd_event_disable(rd_agent_t *rd)
{
	/*
	 * Tell the event callback that we are shutting down.
	 */

	if (rd->rd_event_fun)
		rd->rd_event_fun(rd, NULL, rd->rd_event_data);

	rd->rd_event_fun = NULL;
	rd->rd_event_data = NULL;
	if (rd->rd_monitoring && rd->no_inconsistent == 0) {
		Punbkpt(rd->P, rd->r_brk_addr);
		rd->rd_monitoring = 0;
	}

	_dprintf("%i: disabled rtld activity monitoring.\n", rd->P->pid);
}

/*
 * Enable iteration over the link maps.
 *
 * Note: RD_NOMAPS can be returned at any time, even after some maps have been
 * iterated over.
 */
rd_err_e
rd_loadobj_iter(rd_agent_t *rd, rl_iter_f *fun, void *state)
{
	uintptr_t old_r_brk;
	Lmid_t lmid;
	int nonzero_consistent = FALSE;
	size_t nns;
	int found_any = FALSE;
	size_t num = 0;
	jmp_buf * volatile old_exec_jmp;
	jmp_buf **jmp_pad, this_exec_jmp;
	rd_loadobj_t obj = {0};
	uintptr_t *primary_scope = NULL;
	uintptr_t primary_nscopes = 0; /* quash a warning */

	/*
	 * Trap exec()s at any point within this code.
	 */
	jmp_pad = libproc_unwinder_pad(rd->P);
	old_exec_jmp = *jmp_pad;
	if (setjmp(this_exec_jmp))
		goto spotted_exec;
	*jmp_pad = &this_exec_jmp;

	Pwait(rd->P, 0);

	if (rd->P->state == PS_DEAD) {
		*jmp_pad = old_exec_jmp;

		_dprintf("%i: link map iteration failed: process is dead..\n",
		    rd->P->pid);
		return RD_ERR;
	}

	if (r_brk(rd) == 0 || !rd->maps_ready) {
		*jmp_pad = old_exec_jmp;

		_dprintf("%i: link map iteration failed: maps are not ready.\n",
		    rd->P->pid);
		return RD_NOMAPS;
	}

	if (rd_ldso_consistent_begin(rd) != 0) {
		*jmp_pad = old_exec_jmp;

		_dprintf("%i: link map iteration failed: cannot wait for "
		    "consistent state.\n", rd->P->pid);
		return RD_ERR;
	}

	nns = dl_nns(rd);

	_dprintf("%i: iterating over link maps in %li namespaces.\n",
	    rd->P->pid, nns);

	for (lmid = 0; lmid < nns; lmid++) {

		uintptr_t loadobj;
		unsigned int nloaded = 0;
		unsigned int n = 0;

		if (!nonzero_consistent && nns > 1) {
			nonzero_consistent = TRUE;
			if (rd_ldso_nonzero_lmid_consistent_begin(rd) < 0)
				goto err;
		}

		if (lmid > 0) {
			nloaded = ns_nloaded(rd, lmid);
			_dprintf("%i: %i objects in LMID %li\n", rd->P->pid,
			    nloaded, lmid);
		}

		Pwait(rd->P, FALSE);

		/*
		 * Read this link map out of the child.  If link map zero cannot
		 * be read, the link maps are not yet ready.
		 */

		loadobj = first_link_map(rd, lmid);

		if (lmid == 0 && loadobj == 0)
			goto err;

		if ((loadobj == 0) && (lmid == LM_ID_BASE)) {

			if (nonzero_consistent)
				rd_ldso_nonzero_lmid_consistent_end(rd);
			rd_ldso_consistent_end(rd);

			*jmp_pad = old_exec_jmp;
			_dprintf("%i: link map iteration: no maps.\n", rd->P->pid);
			return RD_NOMAPS;
		}

		while ((loadobj != 0) &&
		    (lmid == 0 || (lmid > 0 && n < nloaded))) {
			struct link_map map;
			uintptr_t *real_scope = NULL;
			unsigned int real_nscopes;

			found_any = TRUE;

			if (rd_get_link_map(rd, &map, loadobj) == NULL)
			    goto err;

			obj.rl_lmident = lmid;
			if (rd_get_loadobj_link_map(rd, &obj, &map,
				loadobj) == NULL)
				goto err;

			/*
			 * If this is the first library in this lmid, its
			 * searchlist is used as the default for all those
			 * libraries that do not have searchlists already.
			 */
			if (!primary_scope) {
				size_t nscopes_size = obj.rl_nscopes *
				    sizeof(uintptr_t);

				primary_scope = malloc(nscopes_size);
				primary_nscopes = obj.rl_nscopes;

				if (!primary_scope) {
					fprintf(stderr, "Out of memory allocating "
					    "primary scopes list of length %lu "
					    "bytes\n", nscopes_size);
					exit(1);
				}
				memcpy(primary_scope, obj.rl_scope,
				    nscopes_size);
			}

			if (obj.rl_nscopes == 0) {
				real_scope = obj.rl_scope;
				real_nscopes = obj.rl_nscopes;
				obj.rl_scope = primary_scope;
				obj.rl_nscopes = primary_nscopes;
				obj.rl_default_scope = 1;
			} else
				obj.rl_default_scope = 0;

			if (fun(&obj, num, state) == 0) {
				if (real_scope)
					obj.rl_scope = real_scope;
				goto err;
			}

			if (real_scope) {
				obj.rl_scope = real_scope;
				obj.rl_nscopes = real_nscopes;
			}

			num++;
			n++;

			loadobj = (uintptr_t) map.l_next;
		}

		/*
		 * The assignments to NULL below are crucial -- we could spot an
		 * exec() after this point, and longjmp() into spotted_exec:,
		 * which free()s them again.
		 */

		free(primary_scope);
		primary_scope = NULL;
	}

	free(obj.rl_scope);
	obj.rl_scope = NULL;

	if (nonzero_consistent)
		rd_ldso_nonzero_lmid_consistent_end(rd);
	rd_ldso_consistent_end(rd);

	*jmp_pad = old_exec_jmp;

	if (!found_any) {
		_dprintf("%i: link map iteration: no maps.\n", rd->P->pid);
		return RD_NOMAPS;
	}

	return RD_OK;

err:
	if (nonzero_consistent)
		rd_ldso_nonzero_lmid_consistent_end(rd);
	rd_ldso_consistent_end(rd);

	/*
	 * It is possible we have just died or exec()ed in the middle of
	 * iteration.  Pwait() to pick that up.
	 */

	free(primary_scope);
	free(obj.rl_scope);
	primary_scope = NULL;
	obj.rl_scope = NULL;

	old_r_brk = r_brk(rd);
	Pwait(rd->P, FALSE);

	jmp_pad = libproc_unwinder_pad(rd->P);
	*jmp_pad = old_exec_jmp;

	if (rd->P->state == PS_DEAD ||
	    r_brk(rd) == 0 || r_brk(rd) != old_r_brk) {
		_dprintf("%i: link map iteration failed: maps are not ready.\n",
		    rd->P->pid);
		return RD_NOMAPS;
	}

	_dprintf("%i: link map iteration: read from child failed.\n", rd->P->pid);
	return RD_ERR;

 spotted_exec:
	_dprintf("%i: spotted exec() in rd_loadobj_iter()\n", rd->P->pid);
	free(primary_scope);
	free(obj.rl_scope);
	if (old_exec_jmp)
		longjmp(*old_exec_jmp, 1);

	jmp_pad = libproc_unwinder_pad(rd->P);
	*jmp_pad = old_exec_jmp;
	return RD_NOMAPS;
}

/*
 * Look up a scope element in a loadobj structure and a second loadobj structure
 * to populate.
 *
 * The rl_scope array in the returned structure is realloc()ed as needed, should
 * be NULL to start with, and must be freed by the caller when done.
 */
struct rd_loadobj *
rd_get_scope(rd_agent_t *rd, rd_loadobj_t *buf, const rd_loadobj_t *obj,
    unsigned int scope)
{
	struct link_map map;

	if (scope > obj->rl_nscopes)
		return NULL;

	if (rd_get_link_map(rd, &map, obj->rl_scope[scope]) == NULL)
		return NULL;

	if (rd_get_loadobj_link_map(rd, buf, &map,
		obj->rl_scope[scope]) == NULL)
		return NULL;

	buf->rl_lmident = obj->rl_lmident;

	return buf;
}
