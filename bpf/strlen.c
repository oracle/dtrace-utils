// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <stdint.h>
#include <dt_dctx.h>
#include <bpf-lib.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern uint64_t STBSZ;
extern uint64_t STRSZ;

noinline uint64_t dt_strlen(const dt_dctx_t *dctx, const char *str)
{
	char	*tmp = dctx->strtab + (uint64_t)&STBSZ;
	int64_t	len;

	/*
	 * The bpf_probe_read_str() helper returns either a negative value (for
	 * error conditions) or a positive value (string length + 1 to account
	 * for the terminating 0-byte).  It will never return 0, so it is safe
	 * to speculatively subtract 1.  Any negative value will be converted
	 * into a 0.
	 */
	len = bpf_probe_read_str(tmp, (uint64_t)&STRSZ + 1, str) - 1;
	set_not_neg_bound(len);

	return len;
}
