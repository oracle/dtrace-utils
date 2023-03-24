/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into an associate array element removes it from
 *	      storage, making room for another element.
 *
 * SECTION: Variables/Thread-Local Variables
 */

/*
 * We need enough storage to store 4 dynamic variables.  Each dynamic variable
 * will require 20 bytes of storage:
 *	0..3   = Variable ID
 *	4..7   = Index
 *	8..15  = 0
 *	16..19 = Value
 */
#pragma D option dynvarsize=80
#pragma D option quiet

BEGIN
{
	a[1] = 1;
	a[2] = 2;
	a[3] = 3;
	a[1] = 0;
	a[4] = 4;
	trace(a[1]);
	trace(a[2]);
	trace(a[3]);
	trace(a[4]);
	exit(0);
}

ERROR
{
	exit(1);
}
