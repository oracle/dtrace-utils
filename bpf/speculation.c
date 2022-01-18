// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf-helpers.h>
#include <dt_bpf_maps.h>

#include <dtrace/faults_defines.h>
#include <asm/errno.h>
#include <stdatomic.h>

#include "probe_error.h"

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def specs;
extern uint64_t NSPEC;

/*
 * Assign a speculation ID.
 */
noinline uint32_t dt_speculation(void)
{
	uint32_t id;
	dt_bpf_specs_t zero;

	__builtin_memset(&zero, 0, sizeof (dt_bpf_specs_t));

#if 1 /* Loops are broken in BPF right now */
#define SEARCH(n)							\
	do {								\
		if (n > (uint64_t) &NSPEC)				\
			return 0;					\
		id = (n);						\
		if (bpf_map_update_elem(&specs, &id, &zero,		\
			BPF_NOEXIST) == 0)				\
			return id;					\
	} while (0);

	SEARCH(1);
	SEARCH(2);
	SEARCH(3);
	SEARCH(4);
	SEARCH(5);
	SEARCH(6);
	SEARCH(7);
	SEARCH(8);
	SEARCH(9);
	SEARCH(10);
	SEARCH(11);
	SEARCH(12);
	SEARCH(13);
	SEARCH(14);
	SEARCH(15);
	SEARCH(16);
#else
	/*
	 * Waiting on a verifier that can handle loops
	 */
	for (id = 1; id <= (uint64_t) &NSPEC; id++) {
		if (bpf_map_update_elem(&specs, &id, &zero,
					BPF_NOEXIST) == 0)
			return id;
	}
#endif

	return 0;
}

/*
 * Begin a speculation given an already-assigned ID.
 *
 * We consider a speculation ID usable only if it exists in the speculation map
 * (indicating that speculation() has returned it) with a a zero read value
 * (indicating that neither commit() nor discard() have been called for it).
 * We bump the written value by one to indicate that another speculative buffer
 * is (or will soon be, once this clause terminates and its epilogue runs)
 * available for draining.
 */
noinline int32_t
dt_speculation_speculate(uint32_t id)
{
	dt_bpf_specs_t *spec;

	if ((spec = bpf_map_lookup_elem(&specs, &id)) == NULL)
		return -1;

	/*
	 * Spec already being drained: do not continue to emit new
	 * data into it.
	 */
	if (spec->draining)
		return -1;

	spec->written++;
	/* Use atomics once GCC/binutils can emit them in forms that older
	 * kernels support:
	 * __atomic_add_fetch(&spec->written, 1, __ATOMIC_RELAXED);
	 */
	return 0;
}

/*
 * Mark a committed or discarded speculation as drainable by userspace.
 *
 * Once drained, the speculation ID is freed and may be reused.
 */

noinline int32_t
dt_speculation_set_drainable(const dt_dctx_t *dctx, uint32_t id)
{
	dt_bpf_specs_t *spec;

	/*
	 * Arguably redundant to BPF_EXIST, but that doesn't seem to return
	 * -EEXIST as one might hope.
	 */
	if ((spec = bpf_map_lookup_elem(&specs, &id)) == NULL) {
		if (id <= (uint64_t) &NSPEC)
			return 0;
		dt_probe_error(dctx, -1, DTRACEFLT_ILLOP, 0);
		return -1;
	}
	spec->draining = 1;

	return 0;
}
