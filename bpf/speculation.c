// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf-helpers.h>
#include <dt_bpf_maps.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def specs;
extern uint64_t NSPEC;

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
