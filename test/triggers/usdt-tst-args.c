/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include "usdt-tst-args-prov.h"

int
main(int argc, char **argv)
{
	for (;;) {
		TEST_PROV_PLACE(10, 4, 20, 30, 40, 50, 60, 70, 80, 90);
	}

	return 0;
}
