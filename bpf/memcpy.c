// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

noinline int dt_memcpy_int(void *dst, void *src, size_t n)
{
	uint64_t	*d = dst;
	uint64_t	*s = src;
	size_t		r, i;

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
		((uint32_t *)dst)[i] = ((uint32_t *)src)[i];
		r -= 4;
	}
	if (r >= 2) {
		i = (n / 2) - 1;
		((uint16_t *)dst)[i] = ((uint16_t *)src)[i];
		r -= 2;
	}
	if (r) {
		i = n - 1;
		((uint8_t *)dst)[i] = ((uint8_t *)src)[i];
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
	uint64_t	*d = dst;
	uint64_t	*s = src;

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
