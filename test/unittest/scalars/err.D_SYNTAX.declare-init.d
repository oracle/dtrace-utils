/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare and init a variable Inside Begin and make sure compilation fails.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */

BEGIN
{
	int x = 123;
}
