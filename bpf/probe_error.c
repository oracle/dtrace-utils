// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <dt_dctx.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern int64_t dt_error(const dt_dctx_t *dctx);

/*
 * DTrace ERROR probes provide 6 arguments:
 *	arg0 = always NULL (used to be kernel consumer state pointer)
 *	arg1 = probe ID that triggered the fault
 *	arg2 = statement ID that triggered the fault
 *	arg3 = BPF offset in the clause that triggered the fault (or -1)
 *	arg4 = fault type
 *	arg5 = fault-specific value (usually address being accessed or 0)
 */
noinline void dt_probe_error(const dt_dctx_t *dctx, uint64_t pc, uint64_t fault,
			     uint64_t illval)
{
	dt_mstate_t	*mst = dctx->mst;
	int		oldprid = mst->prid;

	__builtin_memcpy(mst->saved_argv, mst->argv, sizeof(mst->saved_argv));
	mst->argv[0] = 0;
	mst->argv[1] = mst->prid;
	mst->argv[2] = mst->stid;
	mst->argv[3] = pc;
	mst->argv[4] = fault;
	mst->argv[5] = illval;

	mst->prid = DTRACE_ERROR_ID;
	dt_error(dctx);
	mst->prid = oldprid;

	__builtin_memcpy(mst->argv, mst->saved_argv, sizeof(mst->saved_argv));
	mst->fault = fault;
}
