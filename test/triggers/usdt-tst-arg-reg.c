/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#define _DT_ARG_CONSTRAINT	r
#include "usdt-tst-arg-reg-prov.h"

int
main(int argc, char **argv)
{
	uint8_t		a = 0x12;
	int8_t		b = -0x12;
	uint16_t	c = 0x1234;
	int16_t		d = -0x1234;
	uint32_t	e = 0x12345678;
	int32_t		f = -0x12345678;
	uint64_t	g = 0x1234567890abcdefULL;
	int64_t		h = -0x1234567890abcdefLL;

	for (;;) {
		TEST_PROV_UVAL1(a);
		TEST_PROV_SVAL1(b);
		TEST_PROV_UVAL2(c);
		TEST_PROV_SVAL2(d);
		TEST_PROV_UVAL4(e);
		TEST_PROV_SVAL4(f);
		TEST_PROV_UVAL8(g);
		TEST_PROV_SVAL8(h);
	}

	return 0;
}
