/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A clause-local variable declared without a type is implicitly
 *	      declared as an int.  Verify that assignment of a string value
 *	      fails.
 *
 * SECTION: Variables/Clause-Local Variables
 */

this x;

BEGIN
{
	this->x = execname;
	exit(0);
}
