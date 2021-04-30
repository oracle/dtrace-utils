/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <stdint.h>
#include <dt_varint.h>

uint64_t
dt_int2vint(uint64_t val, char *str)
{
	uint8_t	*buf = (uint8_t *)str;
	int	len;

	if (val <= VARINT_1_MAX) {
		buf[0] = (uint8_t)val;
		return 1;
	} else if (val <= VARINT_2_MAX) {
		val -= VARINT_2_MIN;
		len = 2;
		buf[0] = VARINT_2_PREFIX | (uint8_t)(val >> VARINT_2_SHIFT);
		goto two;
	} else if (val <= VARINT_3_MAX) {
		val -= VARINT_3_MIN;
		len = 3;
		buf[0] = VARINT_3_PREFIX | (uint8_t)(val >> VARINT_3_SHIFT);
		goto three;
	} else if (val <= VARINT_4_MAX) {
		val -= VARINT_4_MIN;
		len = 4;
		buf[0] = VARINT_4_PREFIX | (uint8_t)(val >> VARINT_4_SHIFT);
		goto four;
	} else if (val <= VARINT_5_MAX) {
		val -= VARINT_5_MIN;
		len = 5;
		buf[0] = VARINT_5_PREFIX | (uint8_t)(val >> VARINT_5_SHIFT);
		goto five;
	} else if (val <= VARINT_6_MAX) {
		val -= VARINT_6_MIN;
		len = 6;
		buf[0] = VARINT_6_PREFIX | (uint8_t)(val >> VARINT_6_SHIFT);
		goto six;
	} else if (val <= VARINT_7_MAX) {
		val -= VARINT_7_MIN;
		len = 7;
		buf[0] = VARINT_7_PREFIX | (uint8_t)(val >> VARINT_7_SHIFT);
		goto seven;
	} else if (val <= VARINT_8_MAX) {
		val -= VARINT_8_MIN;
		len = 8;
		buf[0] = VARINT_8_PREFIX | (uint8_t)(val >> VARINT_8_SHIFT);
		goto eight;
	}

	val -= VARINT_9_MIN;
	len = 9;
	buf[0] = VARINT_9_PREFIX;
	buf[8] = (uint8_t)((val >> 56) & 0xff);
eight:
	buf[7] = (uint8_t)((val >> 48) & 0xff);
seven:
	buf[6] = (uint8_t)((val >> 40) & 0xff);
six:
	buf[5] = (uint8_t)((val >> 32) & 0xff);
five:
	buf[4] = (uint8_t)((val >> 24) & 0xff);
four:
	buf[3] = (uint8_t)((val >> 16) & 0xff);
three:
	buf[2] = (uint8_t)((val >> 8) & 0xff);
two:
	buf[1] = (uint8_t)(val & 0xff);

	return len;
}

uint64_t
dt_vint2int(const char *str)
{
	const uint8_t	*buf = (const uint8_t *)str;
	uint64_t	hi = buf[0];
	uint64_t	base;
	uint64_t	val = 0;

	if (hi < VARINT_1_PLIM)	  /* 0xxxxxxx -> 0x00 - 0x7f */
		return hi;
	if (hi < VARINT_2_PLIM) { /* 10xxxxxx -> 0x0080 - 0x407f */
		hi &= VARINT_HI_MASK(VARINT_2_PLIM);
		hi <<= VARINT_2_SHIFT;
		base = VARINT_2_MIN;
		goto two;
	}
	if (hi < VARINT_3_PLIM) { /* 110xxxxx -> 0x4080 - 0x20407f */
		hi &= VARINT_HI_MASK(VARINT_3_PLIM);
		hi <<= VARINT_3_SHIFT;
		base = VARINT_3_MIN;
		goto three;
	}
	if (hi < VARINT_4_PLIM) { /* 1110xxxx -> 0x204080 - 0x1020407f */
		hi &= VARINT_HI_MASK(VARINT_4_PLIM);
		hi <<= VARINT_4_SHIFT;
		base = VARINT_4_MIN;
		goto four;
	}
	if (hi < VARINT_5_PLIM) { /* 11110xxx -> 0x10204080 - 0x081020407f */
		hi &= VARINT_HI_MASK(VARINT_5_PLIM);
		hi <<= VARINT_5_SHIFT;
		base = VARINT_5_MIN;
		goto five;
	}
	if (hi < VARINT_6_PLIM) { /* 111110xx -> 0x0810204080 - 0x4081020407f */
		hi &= VARINT_HI_MASK(VARINT_6_PLIM);
		hi <<= VARINT_6_SHIFT;
		base = VARINT_6_MIN;
		goto six;
	}
	if (hi < VARINT_7_PLIM) { /* 1111110x -> 0x40810204080 - 0x204081020407f */
		hi &= VARINT_HI_MASK(VARINT_7_PLIM);
		hi <<= VARINT_7_SHIFT;
		base = VARINT_7_MIN;
		goto seven;
	}
	if (hi < VARINT_8_PLIM) { /* 11111110 -> 0x2040810204080 - 0x10204081020407f */
		hi = 0;
		base = VARINT_8_MIN;
		goto eight;
	}

	/* 11111111 -> 0x102040810204080 - 0xffffffffffffffff */
	hi = 0;
	base = VARINT_9_MIN;

	val += ((uint64_t)buf[8]) << 56;
eight:
	val += ((uint64_t)buf[7]) << 48;
seven:
	val += ((uint64_t)buf[6]) << 40;
six:
	val += ((uint64_t)buf[5]) << 32;
five:
	val += ((uint64_t)buf[4]) << 24;
four:
	val += ((uint64_t)buf[3]) << 16;
three:
	val += ((uint64_t)buf[2]) << 8;
two:
	val += (uint64_t)buf[1];
	val += hi;

	return base + val;
}

uint64_t
dt_vint_size(uint64_t val)
{
	if (val <= VARINT_1_MAX)
		return 1;
	if (val <= VARINT_2_MAX)
		return 2;
	if (val <= VARINT_3_MAX)
		return 3;
	if (val <= VARINT_4_MAX)
		return 4;
	if (val <= VARINT_5_MAX)
		return 5;
	if (val <= VARINT_6_MAX)
		return 6;
	if (val <= VARINT_7_MAX)
		return 7;
	if (val <= VARINT_8_MAX)
		return 8;

	return 9;
}

const char *
dt_vint_skip(const char *str)
{
	const uint8_t	*buf = (const uint8_t *)str;
	uint64_t	hi = buf[0];

	if (hi < VARINT_1_PLIM)	 /* 0xxxxxxx -> 0x00 - 0x7f */
		return &str[1];
	if (hi < VARINT_2_PLIM)  /* 10xxxxxx -> 0x0080 - 0x407f */
		return &str[2];
	if (hi < VARINT_3_PLIM)  /* 110xxxxx -> 0x4080 - 0x20407f */
		return &str[3];
	if (hi < VARINT_4_PLIM)  /* 1110xxxx -> 0x204080 - 0x1020407f */
		return &str[4];
	if (hi < VARINT_5_PLIM)  /* 11110xxx -> 0x10204080 - 0x081020407f */
		return &str[5];
	if (hi < VARINT_6_PLIM)  /* 111110xx -> 0x0810204080 - 0x4081020407f */
		return &str[6];
	if (hi < VARINT_7_PLIM)  /* 1111110x -> 0x40810204080 - 0x204081020407f */
		return &str[7];
	if (hi < VARINT_8_PLIM)  /* 11111110 -> 0x2040810204080 - 0x10204081020407f */
		return &str[8];

	return &str[9];
}
