/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Values up to INT32_MAX can be stored in registers using the
 *	      MOV_IMM instruction, while values greater than INT32_MAX must
 *	      be stored using LDDW.  Values greater than INT32_MAX but smaller
 *	      than UINT32_MAX require LDDW due to sign extending that MOV_IMM
 *	      performs.
 */

#pragma D option quiet

BEGIN
{
	n = 2147483647ul;
	trace(n);
	n = 2147483648ul;
	trace(n);
	n = 2147483649ul;
	trace(n);
	exit(0);
}
