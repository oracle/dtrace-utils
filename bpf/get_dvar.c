// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <bpf-lib.h>
#include <dt_dctx.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def dvars;
extern struct bpf_map_def state;
extern struct bpf_map_def tuples;
extern uint64_t NCPUS;

/*
 * Register a adynamic variable drop.
 */
noinline void *dt_no_dvar(void)
{
	uint32_t	kind = DT_STATE_DYNVAR_DROPS;
	uint32_t	*valp;

	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == 0)
		return 0;

	atomic_add32(valp, 1);
	return 0;
}

/*
 * Dynamic variables are identified using a unique 64-bit key.  Three different
 * categories of dynamic variables are supported in DTrace:
 *
 * 1. Thread-local storage (TLS) variables:
 *	dvar key = TLS key (highest bit = 0)
 * 2. Global associative array elements:
 *	dvar key = &tuples[var id, tuple, (uint64_t)0] (highest bit = 1)
 * 3. TLS associative array elements:
 *	dvar key = &tuples[var id, tuple, TLS key] (highest bit = 1)
 *
 * Given that the TLS key can never be 0, uniqueness of the dvar key is
 * guaranteed in this scheme.
 */

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
	key = ((key & 0x7fffffff) << 32) | id;

	return key;
}

noinline void *dt_get_dvar(uint64_t key, uint64_t store, uint64_t nval,
			   const char *dflt)
{
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
	if (bpf_map_update_elem(&dvars, &key, dflt, BPF_ANY) < 0)
		return dt_no_dvar();

	val = bpf_map_lookup_elem(&dvars, &key);
	if (val != 0)
		return val;

	return dt_no_dvar();
}

noinline void *dt_get_tvar(uint32_t id, uint64_t store, uint64_t nval,
			   const char *dflt)
{
	return dt_get_dvar(dt_tlskey(id), store, nval, dflt);
}

noinline void *dt_get_assoc(uint32_t id, const char *tuple, uint64_t store,
			    uint64_t nval, const char *dflt)
{
	uint64_t	*valp;
	uint64_t	val;
	uint64_t	dflt_val = 0;

	/*
	 * Place the variable ID at the beginning of the tuple.
	 */
	*(uint32_t *)tuple = id;

	/*
	 * Look for the tuple in the tuples map.
	 */
	valp = bpf_map_lookup_elem(&tuples, tuple);
	if (valp == 0) {
		/*
		 * Not found.  If we are not storing a value (i.e. performing a
		 * load), return the default value (0).  If we are trying to
		 * delete an associative array element, we don't have to do
		 * anything because it does not exist anyway.
		 */
		if (!store || !nval)
			return 0;

		/*
		 * Create the tuple and use the address of the value as the
		 * actual value.
		 */
		if (bpf_map_update_elem(&tuples, tuple, &dflt_val, BPF_ANY) < 0)
			return dt_no_dvar();
		valp = bpf_map_lookup_elem(&tuples, tuple);
		if (valp == 0)
			return dt_no_dvar();
		*valp = (uint64_t)valp;
		if (bpf_map_update_elem(&tuples, tuple, valp, BPF_ANY) < 0)
			return dt_no_dvar();

		val = *valp;
	} else {
		/*
		 * Record the value (used as key into the dvars map), and if we
		 * are storing a zero-value (deleting the element), delete the
		 * tuple.  The associated dynamic variable will be deleted by
		 * the dt_get_dvar() call.
		 */
		val = *valp;

		if (store && !nval)
			bpf_map_delete_elem(&tuples, tuple);
	}

	return dt_get_dvar(val, store, nval, dflt);
}
