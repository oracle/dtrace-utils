/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Attempt to create an invalid inline definition by creating an
 * inline of a function type.  This should fail to compile.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES:
 *
 */

inline cyc_func_t i = "i am a cyclic function";
