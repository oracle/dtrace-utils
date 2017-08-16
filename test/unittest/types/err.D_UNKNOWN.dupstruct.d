/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Attempt a bogus struct declaration that contains duplicated member names.
 *
 * SECTION: Structs and Unions/Structs
 */

struct foo {
	int x;
	char x;
};
