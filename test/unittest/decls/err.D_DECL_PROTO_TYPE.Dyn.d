/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Assert that a function parameter in a declaration may not use a dynamic
 * DTrace type such as an associative array type.
 */

int a(int a[int]);

BEGIN,
ERROR
{
	exit(0);
}
