/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Define an enumerations with a overflow value using negative value.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */


enum e {
	toosmall = -2147483649
};
