/*
 * Oracle Linux DTrace; assist seccomp, fool the compiler
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>

void *seccomp_fill_memory_really_malloc(size_t size)
{
	return malloc(size);
}

void seccomp_fill_memory_really_free(void *ptr)
{
	free(ptr);
}
