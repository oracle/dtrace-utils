// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stddef.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <dtrace/conf.h>
#include <dtrace/dif_defines.h>
#include <dtrace/faults_defines.h>
#include <dt_bpf_maps.h>
#include <dt_dctx.h>
#include <dt_state.h>

#include "probe_error.h"

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def	cpuinfo;
extern struct bpf_map_def	probes;
extern struct bpf_map_def	state;
extern struct bpf_map_def	usdt_names;

extern uint64_t	BOOTTM;
extern uint64_t	NPROBES;
extern uint64_t	PC;
extern uint64_t	STBSZ;
extern uint64_t	STKSIZ;
extern uint64_t STACK_OFF;
extern uint64_t	STACK_SKIP;
extern uint64_t	TASK_COMM;
extern uint64_t	TASK_REAL_PARENT;
extern uint64_t	TASK_TGID;

#define error(dctx, fault, illval) \
	({ \
		dt_probe_error((dctx), (uint64_t)&PC, (fault), (illval)); \
		-1; \
	})

noinline uint64_t dt_bvar_args(const dt_dctx_t *dctx, uint32_t idx)
{
	dt_mstate_t	*mst = dctx->mst;

	if (idx >= sizeof(mst->argv) / sizeof(mst->argv[0]))
		return error(dctx, DTRACEFLT_ILLOP, 0);

	return mst->argv[idx];
}

noinline uint64_t dt_bvar_caller(const dt_dctx_t *dctx)
{
	uint64_t	buf[2] = { 0, };

	if (bpf_get_stack(dctx->ctx, buf, sizeof(buf),
			  (uint64_t)(&STACK_SKIP) & BPF_F_SKIP_FIELD_MASK) < 0)
		return 0;

	return buf[1];
}

noinline uint64_t dt_bvar_curcpu(const dt_dctx_t *dctx)
{
	uint32_t	key = 0;
	void		*val = bpf_map_lookup_elem(&cpuinfo, &key);

	if (val == NULL) {
		/*
		 * Typically, we would use 'return error(...);' but
		 * that confuses the verifier because it returns -1.
		 * So, instead, we explicitly return 0.
		 */
		error(dctx, DTRACEFLT_ILLOP, 0);
		return 0;
	}

	return (uint64_t)val;
}

noinline uint64_t dt_bvar_curthread(const dt_dctx_t *dctx)
{
	return bpf_get_current_task();
}

noinline uint64_t dt_bvar_epid(const dt_dctx_t *dctx)
{
	dt_mstate_t	*mst = dctx->mst;

	return (((uint64_t)mst->prid) << 32) | mst->stid;
}

noinline uint64_t dt_bvar_errno(const dt_dctx_t *dctx)
{
	dt_mstate_t	*mst = dctx->mst;

	return mst->syscall_errno;
}

noinline uint64_t dt_bvar_execname(const dt_dctx_t *dctx)
{
	uint64_t	ptr;

	/* &(current->comm) */
	ptr = bpf_get_current_task();
	if (ptr == 0)
		return error(dctx, DTRACEFLT_BADADDR, ptr);

	return (uint64_t)ptr + (uint64_t)&TASK_COMM;
}

noinline uint64_t dt_bvar_gid(const dt_dctx_t *dctx)
{
	return bpf_get_current_uid_gid() >> 32;
}

noinline uint64_t dt_bvar_id(const dt_dctx_t *dctx)
{
	dt_mstate_t	*mst = dctx->mst;

	return mst->prid;
}

noinline uint64_t dt_bvar_pid(const dt_dctx_t *dctx)
{
	return bpf_get_current_pid_tgid() >> 32;
}

noinline uint64_t dt_bvar_ppid(const dt_dctx_t *dctx)
{
	uint64_t	ptr;
	int32_t		val = -1;

	/* Chase pointers val = current->real_parent->tgid. */
	ptr = bpf_get_current_task();
	if (ptr == 0)
		return error(dctx, DTRACEFLT_BADADDR, ptr);
	if (bpf_probe_read((void *)&ptr, 8,
			   (const void *)(ptr + (uint64_t)&TASK_REAL_PARENT)))
		return error(dctx, DTRACEFLT_BADADDR, ptr + (uint64_t)&TASK_REAL_PARENT);
	if (bpf_probe_read((void *)&val, 4,
			   (const void *)(ptr + (uint64_t)&TASK_TGID)))
		return error(dctx, DTRACEFLT_BADADDR, ptr + (uint64_t)&TASK_TGID);

	return (uint64_t)val;
}

noinline uint64_t dt_bvar_probedesc(const dt_dctx_t *dctx, uint32_t idx)
{
	dt_mstate_t			*mst = dctx->mst;
	uint32_t			key = mst->prid;

	if (key < ((uint64_t)&NPROBES)) {
		dt_bpf_probe_t	*pinfo;
		uint64_t	off = 0;

		pinfo = bpf_map_lookup_elem(&probes, &key);
		if (pinfo == NULL)
			return (uint64_t)dctx->strtab;

		switch (idx) {
		case DIF_VAR_PROBEPROV:
			off = pinfo->prv;
			break;
		case DIF_VAR_PROBEMOD:
			off = pinfo->mod;
			break;
		case DIF_VAR_PROBEFUNC:
			off = pinfo->fun;
			break;
		case DIF_VAR_PROBENAME:
			off = pinfo->prb;
		}
		if (off > (uint64_t)&STBSZ)
			return (uint64_t)dctx->strtab;

		return (uint64_t)(dctx->strtab + off);
	} else {
		char	*s;

		s = bpf_map_lookup_elem(&usdt_names, &key);
		if (s == NULL)
			return (uint64_t)dctx->strtab;

		switch (idx) {
		case DIF_VAR_PROBEPROV:
			s += DTRACE_FUNCNAMELEN;
		case DIF_VAR_PROBEMOD:
			s += DTRACE_MODNAMELEN;
		case DIF_VAR_PROBEFUNC:
			s += DTRACE_PROVNAMELEN;
		case DIF_VAR_PROBENAME:
		}

		return (uint64_t)s;
	}
}

noinline uint64_t dt_bvar_stackdepth(const dt_dctx_t *dctx)
{
	uint32_t	bufsiz = (uint32_t) (uint64_t) (&STKSIZ);
	char		*buf = dctx->mem + (uint64_t)(&STACK_OFF);
	uint64_t	retv;

	retv = bpf_get_stack(dctx->ctx, buf, bufsiz,
			     (uint64_t)(&STACK_SKIP) & BPF_F_SKIP_FIELD_MASK);
	if (retv < 0)
		return error(dctx, DTRACEFLT_BADSTACK, 0 /* FIXME */);

	/*
	 * While linux/bpf.h does not describe the meaning of bpf_get_stack()'s
	 * return value outside of its sign, it is presumably the length of the
	 * copied stack.
	 *
	 * If retv==bufsiz, presumably the stack is larger than what we
	 * can retrieve.  But it's also possible that the buffer was exactly
	 * large enough.  So, leave it to the user to interpret the result.
	 */
	return retv / sizeof(uint64_t);
}

noinline uint64_t dt_bvar_tid(const dt_dctx_t *dctx)
{
	return bpf_get_current_pid_tgid() & 0x00000000ffffffffUL;
}

noinline uint64_t dt_bvar_timestamp(const dt_dctx_t *dctx)
{
	dt_mstate_t	*mst = dctx->mst;

	if (mst->tstamp == 0)
		mst->tstamp = bpf_ktime_get_ns();

	return mst->tstamp;
}

noinline uint64_t dt_bvar_ucaller(const dt_dctx_t *dctx)
{
	uint64_t buf[2] = { 0, };

	if (bpf_get_stack(dctx->ctx, buf, sizeof(buf), BPF_F_USER_STACK) < 0)
		return 0;

	return buf[1];
}

noinline uint64_t dt_bvar_uid(const dt_dctx_t *dctx)
{
	return bpf_get_current_uid_gid() & 0x00000000ffffffffUL;
}

noinline uint64_t dt_bvar_ustackdepth(const dt_dctx_t *dctx)
{
	uint32_t	bufsiz = (uint32_t) (uint64_t) (&STKSIZ);
	char		*buf = dctx->mem + (uint64_t)(&STACK_OFF);
	uint64_t	retv;

	retv = bpf_get_stack(dctx->ctx, buf, bufsiz, BPF_F_USER_STACK);
	if (retv < 0)
		return error(dctx, DTRACEFLT_BADSTACK, 0 /* FIXME */);

	/* See dt_bvar_stackdepth() above. */
	return retv / sizeof(uint64_t);
}

noinline uint64_t dt_bvar_walltimestamp(const dt_dctx_t *dctx)
{
	return bpf_ktime_get_ns() + ((uint64_t)&BOOTTM);
}
