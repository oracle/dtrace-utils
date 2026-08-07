// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stddef.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <dtrace/faults_defines.h>
#include <dt_dctx.h>
#include <dt_state.h>

#include "probe_error.h"

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern struct bpf_map_def	state;

extern uint64_t PC;

#define error(dctx, fault, illval) \
	({ \
		dt_probe_error((dctx), (uint64_t)&PC, (fault), (illval)); \
		-1; \
	})

noinline uint64_t dt_bvar_ns_pid(const dt_dctx_t *dctx)
{
	uint32_t		*valp;
	uint32_t		kind, dev, ino;
	struct bpf_pidns_info	info;

	kind = DT_STATE_NS_DEV;
	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == NULL)
		return error(dctx, DTRACEFLT_ILLOP, 0);
	dev = *valp;

	kind = DT_STATE_NS_INO;
	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == NULL)
		return error(dctx, DTRACEFLT_ILLOP, 0);
	ino = *valp;

	if (bpf_get_ns_current_pid_tgid(dev, ino, &info, 8) != 0)
		return 0;

	return info.tgid;
}

noinline uint64_t dt_bvar_ns_tid(const dt_dctx_t *dctx)
{
	uint32_t		*valp;
	uint32_t		kind, dev, ino;
	struct bpf_pidns_info	info;

	kind = DT_STATE_NS_DEV;
	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == NULL)
		return error(dctx, DTRACEFLT_ILLOP, 0);
	dev = *valp;

	kind = DT_STATE_NS_INO;
	valp = bpf_map_lookup_elem(&state, &kind);
	if (valp == NULL)
		return error(dctx, DTRACEFLT_ILLOP, 0);
	ino = *valp;

	if (bpf_get_ns_current_pid_tgid(dev, ino, &info, 8) != 0)
		return 0;

	return info.pid;
}
