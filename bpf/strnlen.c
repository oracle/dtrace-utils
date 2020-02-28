// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

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
noinline int dt_strnlen_dw(const uint64_t *p, size_t n)
{
	uint64_t	v = *p;
	char		*s = (char *)p;

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
	uint64_t	*p = (uint64_t *)s;
	int		l = 0;
	int		n;

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
