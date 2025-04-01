/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include "usdt-tst-arg-const-prov.h"

int
main(int argc, char **argv)
{
	for (;;) {
		TEST_PROV_UVAL1(0x12);
		TEST_PROV_SVAL1(-0x12);
		TEST_PROV_UVAL2(0x1234);
		TEST_PROV_SVAL2(-0x1234);
		TEST_PROV_UVAL4(0x12345678);
		TEST_PROV_SVAL4(-0x12345678);
		TEST_PROV_UVAL8(0x1234567890abcdefULL);
		TEST_PROV_SVAL8(-0x1234567890abcdefLL);
	}

	return 0;
}
