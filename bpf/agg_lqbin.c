// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <stdint.h>
#include <bpf-helpers.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

noinline uint64_t dt_agg_lqbin(int64_t val, int32_t base, uint64_t levels,
			       uint64_t step)
{
	uint64_t level;

	if (step == 0 || val < base)
		return 0;
	level = (val - base) / step;
	if (level > levels)
		level = levels;
	return level + 1;
}
