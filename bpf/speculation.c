// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <bpf-lib.h>
#include <dt_bpf_maps.h>

#include <dtrace/faults_defines.h>
#include <asm/errno.h>
#include <stdatomic.h>

#include "probe_error.h"

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def specs;
extern struct bpf_map_def state;
extern uint64_t PC;
extern uint64_t NSPEC;

/*
 * Register a speculation drop.
 */
noinline uint32_t dt_no_spec(uint32_t kind)
{
	uint32_t	*valp;

	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == 0)
		return 0;

	atomic_add32(valp, 1);
	return 0;
}

/*
 * Assign a speculation ID.
 */
noinline uint32_t dt_speculation(void)
{
	uint32_t	id, busy;
	dt_bpf_specs_t	zero;
	dt_bpf_specs_t	*spec;

	__builtin_memset(&zero, 0, sizeof (dt_bpf_specs_t));

	busy = 0;
#if 1 /* Loops are broken in BPF right now */
#define SEARCH(n)							\
	do {								\
		if (n > (uint64_t) &NSPEC)				\
			goto no_spec;					\
		id = (n);						\
		if (bpf_map_update_elem(&specs, &id, &zero,		\
			BPF_NOEXIST) == 0)				\
			return id;					\
		spec = bpf_map_lookup_elem(&specs, &id);		\
		if (spec != 0 && spec->draining > 0) {			\
			busy++;						\
			break;						\
		}							\
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

		spec = bpf_map_lookup_elem(&specs, &id);
		if (spec != 0 && spec->draining > 0)  {
			busy++;
			break;
		}
	}
#endif

no_spec:
	return dt_no_spec(busy ? DT_STATE_SPEC_BUSY : DT_STATE_SPEC_UNAVAIL);
}

/*
 * Begin a speculation given an already-assigned ID.
 *
 * We consider a speculation ID usable only if it exists in the speculation map
 * (indicating that speculation() has returned it) and it is not being drained
 * (indicating that neither commit() nor discard() have been called for it).
 * We bump the written value by one to indicate that another speculative buffer
 * is (or will soon be, once this clause terminates and its epilogue runs)
 * available for draining.
 *
 * We return the speculation entry in the map or NULL.
 */
noinline dt_bpf_specs_t *
dt_speculation_speculate(uint32_t id)
{
	dt_bpf_specs_t *spec;

	if ((spec = bpf_map_lookup_elem(&specs, &id)) == NULL)
		return NULL;

	/*
	 * Spec already being drained: do not continue to emit new
	 * data into it.
	 */
	if (spec->draining)
		return NULL;

	atomic_add(&spec->written, 1);

	return spec;
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
		dt_probe_error(dctx, (uint64_t)&PC, DTRACEFLT_ILLOP, 0);
		return -1;
	}
	spec->draining = 1;

	return 0;
}
