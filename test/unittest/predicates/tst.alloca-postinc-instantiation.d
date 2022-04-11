/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: alloca assignment to variables in predicates works and
 *	      instantiates the variable in the clause body too, just
 *	      as already happens for postfix increment.  You can store
 *	      and load from it.
 */

#pragma D option quiet

BEGIN / (new++ == 0) ? bar = alloca(10) : bar /
{
	trace(new);
	trace(bar);
	b = (int *) ((long) bar + 1 - 1);
        *b = 0;
        b_value = *b;
}

BEGIN / new == 0 /
{
	trace("new is still zero");
	exit(1);
}

BEGIN / b_value == 0 / { exit(0); }

BEGIN / b == 0 /
{
	printf("b stayed zero");
	exit(1);
}

BEGIN
{
	printf("*b is %x, not zero", b_value);
	exit(1);
}

ERROR { exit(1); }
