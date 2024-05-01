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

extern struct bpf_map_def cpuinfo;
extern struct bpf_map_def probes;
extern struct bpf_map_def state;

extern uint64_t PC;
extern uint64_t STBSZ;
extern uint64_t STKSIZ;
extern uint64_t BOOTTM;
extern uint64_t STACK_OFF;
extern uint64_t STACK_SKIP;

#define error(dctx, fault, illval) \
	({ \
		dt_probe_error((dctx), (uint64_t)&PC, (fault), (illval)); \
		-1; \
	})

noinline uint64_t dt_get_bvar(const dt_dctx_t *dctx, uint32_t id, uint32_t idx)
{
	dt_mstate_t	*mst = dctx->mst;

	switch (id) {
	case DIF_VAR_CURTHREAD:
		return bpf_get_current_task();
	case DIF_VAR_TIMESTAMP:
		if (mst->tstamp == 0)
			mst->tstamp = bpf_ktime_get_ns();

		return mst->tstamp;
	case DIF_VAR_EPID:
		return mst->epid;
	case DIF_VAR_ID:
		return mst->prid;
	case DIF_VAR_ARG0: case DIF_VAR_ARG1: case DIF_VAR_ARG2:
	case DIF_VAR_ARG3: case DIF_VAR_ARG4: case DIF_VAR_ARG5:
	case DIF_VAR_ARG6: case DIF_VAR_ARG7: case DIF_VAR_ARG8:
	case DIF_VAR_ARG9:
		return mst->argv[id - DIF_VAR_ARG0];
	case DIF_VAR_ARGS:
		if (idx >= sizeof(mst->argv) / sizeof(mst->argv[0]))
			return error(dctx, DTRACEFLT_ILLOP, 0);

		return mst->argv[idx];
	case DIF_VAR_STACKDEPTH:
	case DIF_VAR_USTACKDEPTH: {
		uint32_t bufsiz = (uint32_t) (uint64_t) (&STKSIZ);
		uint64_t flags;
		char *buf = dctx->mem + (uint64_t)(&STACK_OFF);
		uint64_t stacksize;

		if (id == DIF_VAR_USTACKDEPTH)
			flags = BPF_F_USER_STACK;
		else
			flags = (uint64_t)(&STACK_SKIP) & BPF_F_SKIP_FIELD_MASK;

		stacksize = bpf_get_stack(dctx->ctx, buf, bufsiz, flags);
		if (stacksize < 0)
			return error(dctx, DTRACEFLT_BADSTACK, 0 /* FIXME */);

		/*
		 * While linux/bpf.h does not describe the meaning of
		 * bpf_get_stack()'s return value outside of its sign,
		 * it is presumably the length of the copied stack.
		 *
		 * If stacksize==bufsiz, presumably the stack is larger than
		 * what we can retrieve.  But it's also possible that the
		 * buffer was exactly large enough.  So, leave it to the user
		 * to interpret the result.
		 */
		return stacksize / sizeof(uint64_t);
	}
	case DIF_VAR_CALLER:
	case DIF_VAR_UCALLER: {
		uint64_t flags;
		uint64_t buf[2] = { 0, };

		if (id == DIF_VAR_UCALLER)
			flags = BPF_F_USER_STACK;
		else
			flags = (uint64_t)(&STACK_SKIP) & BPF_F_SKIP_FIELD_MASK;

		if (bpf_get_stack(dctx->ctx, buf, sizeof(buf), flags) < 0)
			return 0;
		return buf[1];
	}
	case DIF_VAR_PROBEPROV:
	case DIF_VAR_PROBEMOD:
	case DIF_VAR_PROBEFUNC:
	case DIF_VAR_PROBENAME: {
		uint32_t	key;
		dt_bpf_probe_t	*pinfo;
		uint64_t	off;

		key = mst->prid;
		pinfo = bpf_map_lookup_elem(&probes, &key);
		if (pinfo == NULL)
			return (uint64_t)dctx->strtab;

		switch (id) {
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
	}
	case DIF_VAR_PID: {
		uint64_t	val = bpf_get_current_pid_tgid();

		return val >> 32;
	}
	case DIF_VAR_TID: {
		uint64_t	val = bpf_get_current_pid_tgid();

		return val & 0x00000000ffffffffUL;
	}
	case DIF_VAR_EXECNAME: {
		uint64_t	ptr;
		extern uint64_t	TASK_COMM;

		/* &(current->comm) */
		ptr = bpf_get_current_task();
		if (ptr == 0)
			return error(dctx, DTRACEFLT_BADADDR, ptr);

		return (uint64_t)ptr + (uint64_t)&TASK_COMM;
	}
	case DIF_VAR_WALLTIMESTAMP:
		return bpf_ktime_get_ns() + ((uint64_t)&BOOTTM);
	case DIF_VAR_PPID: {
		uint64_t	ptr;
		int32_t		val = -1;
		extern uint64_t	TASK_REAL_PARENT;
		extern uint64_t	TASK_TGID;

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
	case DIF_VAR_UID: {
		uint64_t	val = bpf_get_current_uid_gid();

		return val & 0x00000000ffffffffUL;
	}
	case DIF_VAR_GID: {
		uint64_t	val = bpf_get_current_uid_gid();

		return val >> 32;
	}
	case DIF_VAR_ERRNO:
		return mst->syscall_errno;
	case DIF_VAR_CURCPU: {
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
	default:
		/* Not implemented yet. */
		return error(dctx, DTRACEFLT_ILLOP, 0);
	}
}
