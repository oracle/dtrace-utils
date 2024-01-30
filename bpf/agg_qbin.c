// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

noinline int16_t dt_agg_qbin(int64_t val)
{
	int16_t off;
	uint64_t tmp;

	if (val == 0)
		return 63;
	if (val == 0x8000000000000000)
		return 0;

	tmp = val;
	if (val < 0)
		tmp *= -1;

	/* now, tmp has at least one 1, while the leading bit is 0 */
	off = 1;
	if (tmp & 0x7fffffff00000000) { off += 32; tmp >>= 32; }
	if (tmp & 0x00000000ffff0000) { off += 16; tmp >>= 16; }
	if (tmp & 0x000000000000ff00) { off +=  8; tmp >>=  8; }
	if (tmp & 0x00000000000000f0) { off +=  4; tmp >>=  4; }
	if (tmp & 0x000000000000000c) { off +=  2; tmp >>=  2; }
	if (tmp & 0x0000000000000002) { off +=  1; }

	if (val < 0)
		off *= -1;
	off += 63;
	return off;
}
