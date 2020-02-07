// SPDX-License-Identifier: GPL-2.0
/*
 * COMPILE ON x86_64 WITH:
 *	bpf-unknown-none-gcc -D__amd64 -I./libdtrace -I/usr/include \
 *		-o build/dlibs/bpf_dlib.o -c libdtrace/bpf_dlib.c
 *
 * COMPILE ON aarch64 WITH:
 *	bpf-unknown-none-gcc -D__aarch64__ -I./libdtrace -I/usr/include \
 *		-o build/dlibs/bpf_dlib.o -c libdtrace/bpf_dlib.c
 *
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 */
#include <linux/bpf.h>
#include <asm/ptrace.h>
#include <stddef.h>
#include <bpf-helpers.h>

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;
typedef unsigned int	uint32_t;
typedef unsigned long	u64;
typedef unsigned long	uint64_t;
#define memset(x, y, z)	__builtin_memset((x), (y), (z))
#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

#include <dt_bpf_ctx.h>

struct syscall_data {
	struct pt_regs *regs;
	long syscall_nr;
	long arg[6];
};

struct bpf_map_def SEC("maps") buffers = {
	.type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u32),
	.max_entries = 16,
};

struct bpf_map_def SEC("maps") ecbs = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u32),
	.max_entries = 16,
};

struct bpf_map_def SEC("maps") mem = {
	.type = BPF_MAP_TYPE_PERCPU_ARRAY,
	.key_size = sizeof(u32),
	.value_size = 2048,
	.max_entries = 1,
};

struct bpf_map_def SEC("maps") gvars = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u64),
	.max_entries = 16,
};

struct bpf_map_def SEC("maps") tvars = {
	.type = BPF_MAP_TYPE_ARRAY,
	.key_size = sizeof(u32),
	.value_size = sizeof(u64),
	.max_entries = 16,
};

noinline u64 dt_get_gvar(u32 id)
{
	u32	*val;

	val = bpf_map_lookup_elem(&gvars, &id);
	return val ? *val : 0;
}

noinline void dt_set_gvar(u32 id, u64 val)
{
	bpf_map_update_elem(&gvars, &id, &val, BPF_ANY);
}

noinline u64 dt_get_tvar(u32 id)
{
	u32	*val;

	val = bpf_map_lookup_elem(&tvars, &id);
	return val ? *val : 0;
}

noinline void dt_set_tvar(u32 id, u64 val)
{
	bpf_map_update_elem(&tvars, &id, &val, BPF_ANY);
}

noinline int dt_memcpy_int(void *dst, void *src, size_t n)
{
	u64	*d = dst;
	u64	*s = src;
	size_t	r, i;

	if (n > 128)
		return -1;

	switch (n / 8) {
	case 16:
		d[15] = s[15];
	case 15:
		d[14] = s[14];
	case 14:
		d[13] = s[13];
	case 13:
		d[12] = s[12];
	case 12:
		d[11] = s[11];
	case 11:
		d[10] = s[10];
	case 10:
		d[9] = s[9];
	case 9:
		d[8] = s[8];
	case 8:
		d[7] = s[7];
	case 7:
		d[6] = s[6];
	case 6:
		d[5] = s[5];
	case 5:
		d[4] = s[4];
	case 4:
		d[3] = s[3];
	case 3:
		d[2] = s[2];
	case 2:
		d[1] = s[1];
	case 1:
		d[0] = s[0];
	}

	r = n % 8;
	if (r >= 4) {
		i = (n / 4) - 1;
		((u32 *)dst)[i] = ((u32 *)src)[i];
		r -= 4;
	}
	if (r >= 2) {
		i = (n / 2) - 1;
		((u16 *)dst)[i] = ((u16 *)src)[i];
		r -= 2;
	}
	if (r) {
		i = n - 1;
		((u8 *)dst)[i] = ((u8 *)src)[i];
	}

	return 0;
}

/*
 * Copy a byte sequence of length n from src to dst.  The function returns 0
 * upon success and -1 when n is greater than 256.  Both src and dst must be on
 * a 64-bit address boundary.
 *
 * The size (n) must be no more than 256.
 */
noinline int dt_memcpy(void *dst, void *src, size_t n)
{
	u64	*d = dst;
	u64	*s = src;

	if (n > 128) {
		if (dt_memcpy_int(d, s, 128))
			return -1;
		n -= 128;
		if (n == 0)
			return 0;

		d += 16;
		s += 16;
	}

	return dt_memcpy_int(d, s, n);
}

/*
 * Determine the length of a string, no longer than a given size.
 *
 * Currently, only strings smaller than 256 characters are supported.
 *
 * Strings are expected to be allocated in 64-bit chunks, which guarantees that
 * every string starts on a 64-bit boundary and that the string data can be
 * read in chunks of 64-bit values.
 *
 * Algorithm based on the strlen() implementation in the GNU C Library, written
 * by Torbjorn Granlund with help from Dan Sahlin.
 */
#define STRNLEN_SUBV	0x0101010101010101UL
#define STRNLEN_MASK	0x8080808080808080UL
noinline int dt_strnlen_dw(const u64 *p, size_t n)
{
	u64	v = *p;
	char	*s = (char *)p;

	if (((v - STRNLEN_SUBV) & ~v & STRNLEN_MASK) != 0) {
		if (s[0] == 0)
			return 0;
		if (s[1] == 0)
			return 1;
		if (s[2] == 0)
			return 2;
		if (s[3] == 0)
			return 3;
		if (s[4] == 0)
			return 4;
		if (s[5] == 0)
			return 5;
		if (s[6] == 0)
			return 6;
		if (s[7] == 0)
			return 7;
	}

	return 8;
}

noinline int dt_strnlen(const char *s, size_t maxlen)
{
	u64	*p = (u64 *)s;
	int	l = 0;
	int	n;

#define STRNLEN_1_DW(p, n, l) \
	do { \
		n = dt_strnlen_dw(p++, 1); \
		l += n; \
		if (n < 8) \
			return l; \
		if ((char *)p - s > maxlen) \
			return -1; \
	} while (0)
#define STRNLEN_4_DW(p, n, l) \
	do { \
		STRNLEN_1_DW(p, n, l); \
		STRNLEN_1_DW(p, n, l); \
		STRNLEN_1_DW(p, n, l); \
		STRNLEN_1_DW(p, n, l); \
	} while (0)

	STRNLEN_4_DW(p, n, l);
	STRNLEN_4_DW(p, n, l);
	STRNLEN_4_DW(p, n, l);
	STRNLEN_4_DW(p, n, l);

	return -1;
}
