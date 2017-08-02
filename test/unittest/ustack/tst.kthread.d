/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  That ustack() on a kernel thread (such as the idle task) does not crash.
 *
 * SECTION:  Types, Operators, and Expressions/Conditional Expressions
 *
 * ORABUG: 17591351
 */

#pragma D option quiet

tick-100msec / pid == 0 / { ustack(); exit(0); }
