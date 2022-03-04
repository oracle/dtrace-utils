/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dt_dctx.h>

int main(void)
{
	printf("Base:       % 4d\n", DT_STK_BASE);
	printf("dctx:       % 4d\n", DT_STK_DCTX);
	printf("sp:         % 4d\n", DT_STK_SP);
	printf("%%r0:        % 4d\n", DT_STK_SPILL(0));
	printf("%%r1:        % 4d\n", DT_STK_SPILL(1));
	printf("%%r2:        % 4d\n", DT_STK_SPILL(2));
	printf("%%r3:        % 4d\n", DT_STK_SPILL(3));
	printf("%%r4:        % 4d\n", DT_STK_SPILL(4));
	printf("%%r5:        % 4d\n", DT_STK_SPILL(5));
	printf("%%r6:        % 4d\n", DT_STK_SPILL(6));
	printf("%%r7:        % 4d\n", DT_STK_SPILL(7));
	printf("%%r8:        % 4d\n", DT_STK_SPILL(8));
	printf("sp-base:    % 4d\n", DT_STK_SP_SLOT(0));
	exit(0);
}
