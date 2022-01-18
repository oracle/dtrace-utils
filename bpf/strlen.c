// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <bpf-helpers.h>
#include <stdint.h>
#include <dt_dctx.h>
#include <bpf-lib.h>

#define DT_STRLEN_BYTES	2

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

extern uint64_t STBSZ;
extern uint64_t STRSZ;

noinline void dt_strlen_store(uint64_t val, char *str)
{
	uint8_t		*buf = (uint8_t *)str;

	buf[0] = (uint8_t)(val >> 8);
	buf[1] = (uint8_t)(val & 0xff);
}

noinline uint64_t dt_strlen(const dt_dctx_t *dctx, const char *str)
{
	char	*tmp = dctx->strtab + (uint64_t)&STBSZ;
	int64_t	len;

	len = bpf_probe_read_str(tmp, (uint64_t)&STRSZ + 1, str + DT_STRLEN_BYTES);
	set_not_neg_bound(len);

	return len - 1;		/* bpf_probe_read_str() never returns 0 */
}
