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

extern struct bpf_map_def tvars;

noinline void dt_set_tvar(uint32_t id, uint64_t val)
{
	bpf_map_update_elem(&tvars, &id, &val, BPF_ANY);
}
