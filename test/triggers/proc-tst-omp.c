/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>

int main(int argc, char **argv)
{
        printf("TEST: start\n");
#pragma omp parallel num_threads(2)
        {
		printf("TEST: underway\n");
        }
	return 0;
}
