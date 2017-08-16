/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/sdt.h>

int
main(int argc, char **argv)
{
	for (;;) {
		DTRACE_PROBE2(test_prov, place, 10, 4);
	}

	return (0);
}
