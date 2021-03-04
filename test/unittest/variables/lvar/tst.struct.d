/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Clause-local variables can be declared with a struct type.
 *
 * SECTION: Variables/Clause-Local Variables
 */

struct foo {
	int a;
	int b;
};
this struct foo x;

BEGIN
{
	this->x.a = 1;
	this->x.b = 5;

	trace(this->x.a);
	trace(this->x.b);

	exit(this->x.a == 1 && this->x.b == 5 ? 0 : 1);
}
