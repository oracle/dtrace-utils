/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into an associative array element removes it from
 *	      storage, making room for another element, even for structs.
 *
 * SECTION: Variables/Associative Arrays
 */

/*
 * We need enough storage to store 4 dynamic variables.  Each dynamic variable
 * will require 20 bytes of storage:
 *	0..3   = Variable ID
 *	4..7   = Index
 *	8..15  = 0
 *	16..19 = Value
 */
#pragma D option dynvarsize=79
#pragma D option quiet

this struct {
	int a;
} val;

BEGIN
{
	this->val.a = 1;
	a[1] = this->val;
	this->val.a = 2;
	a[2] = this->val;
	this->val.a = 3;
	a[3] = this->val;
	a[1] = 0;
	this->val.a = 4;
	a[4] = this->val;
	trace(a[2].a);
	trace(a[3].a);
	trace(a[4].a);
	exit(0);
}

ERROR
{
	exit(1);
}
