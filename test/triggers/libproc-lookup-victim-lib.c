/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <stdio.h>
#include <dlfcn.h>
#include <signal.h>
#include "libproc-lookup-victim.h"

void
lib_func(void)
{
	fprintf(stderr, "%s called: This should never be seen.\n", __func__);
}

void
interposed_func(void)
{
	fprintf(stderr, "Testcase error: interposed func called.\n");
}

void
interpose_func (void)
{
    interposed_func();
}
