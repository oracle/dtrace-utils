/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Attempt a bogus enum declaration by assigning a non-constant expression
 *   to an enumerator.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */

enum e {
	TAG = "i am not an integer constant!"
};
