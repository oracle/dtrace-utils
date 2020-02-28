// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf-helpers.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

struct bpf_map_def gvars;

noinline void dt_set_gvar(uint32_t id, uint64_t val)
{
	bpf_map_update_elem(&gvars, &id, &val, BPF_ANY);
}
