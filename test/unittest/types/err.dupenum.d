/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Attempt a bogus enum declaration that contains duplicated enumerator names.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */

/* @@runtest-opts: -xerrtags */

enum foo {
	x = 3,
	x = 4
};
