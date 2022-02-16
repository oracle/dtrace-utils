// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf-helpers.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def dvars;
extern uint64_t NCPUS;

noinline uint64_t dt_tlskey(uint32_t id)
{
	uint64_t	key;

	key = bpf_get_current_pid_tgid();
	key &= 0x00000000ffffffffUL;
	if (key == 0)
		key = bpf_get_smp_processor_id();
	else
		key += (uint32_t)(uint64_t)&NCPUS;

	key++;
	key = (key << 32) | id;

	return key;
}

noinline void *dt_get_dvar(uint64_t key, uint64_t store, uint64_t nval)
{
	uint64_t	dflt_key = 0;
	void		*val;

	/*
	 * If we are going to store a zero-value, it is a request to delete the
	 * TLS variable.
	 */
	if (store && !nval) {
		bpf_map_delete_elem(&dvars, &key);
		return 0;
	}

	/*
	 * For load or non-zero store, we return the address of the value if
	 * there is one.
	 */
	val = bpf_map_lookup_elem(&dvars, &key);
	if (val != 0)
		return val;

	/*
	 * If we are performing a load (not a store), and no var was found,
	 * we are done.
	 */
	if (!store)
		return 0;

	/*
	 * Not found and we are storing a non-zero value: create the variable
	 * with the default value.
	 */
	val = bpf_map_lookup_elem(&dvars, &dflt_key);
	if (val == 0)
		return 0;

	if (bpf_map_update_elem(&dvars, &key, val, BPF_ANY) < 0)
		return 0;

	val = bpf_map_lookup_elem(&dvars, &key);
	if (val != 0)
		return val;

	return 0;
}

noinline void *dt_get_tvar(uint32_t id, uint64_t store, uint64_t nval)
{
	return dt_get_dvar(dt_tlskey(id), store, nval);
}
