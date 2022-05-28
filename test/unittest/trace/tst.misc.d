/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a variety of trace() action invocations.
 *
 * SECTION: Actions and Subroutines/trace();
 *	Output Formatting/trace()
 *
 * NOTES:
 *   We test things that exercise different kinds of DIFO return types
 *   to ensure each one can be traced.
 */

BEGIN
{
	i = 1;
}


tick-1
/i != 5/
{
	trace("test trace");	/* DT_TYPE_STRING */
	trace(12345);		/* DT_TYPE_INT (constant) */
	trace(x++);		/* DT_TYPE_INT (derived) */
	trace(timestamp);	/* DT_TYPE_INT (variable) */
	trace(`max_pfn);	/* CTF type (by value) */
	trace(*`linux_banner);	/* CTF type (by ref) */
	i++;
}

tick-1
/i == 5/
{
	exit(0);
}
