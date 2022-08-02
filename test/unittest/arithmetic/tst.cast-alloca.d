/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Casting an alloca() pointer to a scalar is allowed.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 */

#pragma D option quiet

BEGIN
{
	ptr = alloca(10);
	val = ((int64_t)ptr) >> 8;
	trace(val);
	exit(0);
}

ERROR
{
	exit(1);
}
