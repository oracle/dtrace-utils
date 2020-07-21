/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'timestamp' variable yields the same value for multiple
 *            invocations within the same clause.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
	this->a = timestamp;
	this->b = timestamp;
	trace(this->b - this->a);
	exit(this->a == this->b ? 0 : 1);
}
