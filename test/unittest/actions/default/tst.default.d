/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The default action is used for the empty clause but not
 *	      for a non-empty clause, even if it does not trace anything.
 *	      A special case is a printf() with no attributes, which is
 *	      a trace record with no extra data in the tracing record;
 *	      make sure the printf() occurs.
 *
 * SECTION: Actions and Subroutines/default
 */

/* should not trace, but should update n */
BEGIN
{ n = 1; }

/* should trace, "hello world" should appear */
tick-14
{ printf("hello world"); }

/* should not trace, but should update n */
tick-13
{ n += 2; }

/* should trace */
tick-12
{ }

/* should not trace, but should update n */
tick-11
{ n += 4; }

/* should trace, and n should be 1+2+4=7 */
tick-10
{ trace(n); }

/* should trace */
tick-10
{ exit(0) }
