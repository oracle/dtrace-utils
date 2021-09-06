// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stddef.h>
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

/* Returns -1 on error.  */

noinline int32_t
dt_speculation_set_drainable(dt_dctx_t *dctx, uint32_t id)
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
