/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates. All rights reserved.
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

/* should not trace, but should set n=1 */
BEGIN
{ n = 1; }

/* should trace, "hello world 1" should appear */
BEGIN
{ printf("hello world %d", n); }

/* should not trace, but should update n+=2 */
BEGIN
{ n += 2; }

/* should trace, and n should be 1+2=3 */
BEGIN
{ printf("%d", n); }

/* should trace, but not report anything */
BEGIN
{ }

/* should not trace, but should update n+=4 */
BEGIN
{ n += 4; }

/* should trace, and n should be 1+2+4=7 */
BEGIN
{ trace(n); }

/* should trace */
BEGIN
{ exit(0) }
