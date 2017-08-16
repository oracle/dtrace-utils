/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/wait.h>
#include <stdlib.h>

#include "usdt-tst-forker-prov.h"

int
main(int argc, char **argv)
{
	int i;

	for (i = 0; i < 10000; i++) {
		FORKER_FIRE();
		if (fork() == 0)
			exit(0);

		(void) wait(NULL);
	}

	return (0);
}
