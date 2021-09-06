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

/*
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

	__atomic_add_fetch(&spec->written, 1, __ATOMIC_RELAXED);
	return 0;
}
