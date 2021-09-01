// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 */
#include <stdint.h>

#define DT_STRLEN_BYTES	2

#ifndef noinline
# define noinline	__attribute__((noinline))
#endif

noinline void dt_strlen_store(uint64_t val, char *str)
{
	uint8_t		*buf = (uint8_t *)str;

	buf[0] = (uint8_t)(val >> 8);
	buf[1] = (uint8_t)(val & 0xff);
}

noinline uint64_t dt_strlen(const char *str)
{
	const uint8_t	*buf = (const uint8_t *)str;

	return ((uint64_t)buf[0] << 8) + (uint64_t)buf[1];
}
