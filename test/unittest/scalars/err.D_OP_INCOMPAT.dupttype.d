/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test assigning a variable two different incompatible types.
 *  Test assigning a variable two different incompatible types.
 *
 * SECTION:  Variables/Thread-Local Variables
 *
 */

BEGIN
{
	self->x = curthread;
	self->x = pid;
}
