/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>
#include <dt_impl.h>

int main(void)
{
	printf("Base:       % 4d\n", DT_STK_BASE);
	printf("ctx:        % 4d\n", DT_STK_CTX);
	printf("dctx:       % 4d\n", DT_STK_DCTX);
	printf("%%r0:        % 4d\n", DT_STK_SPILL(0));
	printf("%%r1:        % 4d\n", DT_STK_SPILL(1));
	printf("%%r2:        % 4d\n", DT_STK_SPILL(2));
	printf("%%r3:        % 4d\n", DT_STK_SPILL(3));
	printf("%%r4:        % 4d\n", DT_STK_SPILL(4));
	printf("%%r5:        % 4d\n", DT_STK_SPILL(5));
	printf("%%r6:        % 4d\n", DT_STK_SPILL(6));
	printf("%%r7:        % 4d\n", DT_STK_SPILL(7));
	printf("%%r8:        % 4d\n", DT_STK_SPILL(8));
	printf("lvar[% 3d]:  % 4d (ID % 3d)\n", -1, DT_STK_LVAR(0) + 1,
	       DT_LVAR_OFF2ID(DT_STK_LVAR(0) + 1));
	printf("lvar[% 3d]:  % 4d (ID % 3d)\n", 0, DT_STK_LVAR(0),
	       DT_LVAR_OFF2ID(DT_STK_LVAR(0)));
	printf("lvar[% 3d]:  % 4d (ID % 3d)\n", 1, DT_STK_LVAR(1),
	       DT_LVAR_OFF2ID(DT_STK_LVAR(1)));
	printf("lvar[% 3d]:  % 4d (ID % 3d)\n", DT_LVAR_MAX,
	       DT_STK_LVAR(DT_LVAR_MAX),
	       DT_LVAR_OFF2ID(DT_STK_LVAR(DT_LVAR_MAX)));
	printf("lvar[% 3d]:  % 4d (ID % 3d)\n", -1,
	       DT_STK_LVAR(DT_LVAR_MAX) - 1,
	       DT_LVAR_OFF2ID(DT_STK_LVAR(DT_LVAR_MAX) - 1));
	printf("scratch:    % 4d .. % 4d\n", DT_STK_LVAR_END - 1,
	       DT_STK_SCRATCH_BASE);
	exit(0);
}
