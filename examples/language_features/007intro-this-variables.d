/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./007intro-this-variables.d
 *
 *  DESCRIPTION
 *    We saw earlier that D supports global variables, allowing
 *    convenient access from every probe and every thread.  In
 *    the same way, however, this causes the danger of different
 *    probes and threads overwriting one another's values.
 *
 *    For variables that are to used only within a set of clauses
 *    for a specific probe, specify "this->".  Such variables have
 *    undefined values at the start of the set of clauses.
 */

/* "this" variables can be declared explicitly */
this int exp;

dtrace:::BEGIN
{
	/* the value is undefined by default, define its value */
	this->exp = 1234;

	/* variables can also be typed implicitly */
	this->imp = 5678;
}

dtrace:::BEGIN
/ 1 == 1 /
{
	/* the variables are available in this later clause for the same probe */
	printf("%d %d\n", this->exp, this->imp);

	/*
	 * There is no need to zero out this-> variables at the end of a
	 * set of clauses:  they will no longer exist anyhow and their
	 * values when they are used in a different set of clauses, for
	 * some other probe firing, will be undefined at first.
	 */

	exit(0);
}

END
{
	/* the variables are not available here, for a different probe */
}
