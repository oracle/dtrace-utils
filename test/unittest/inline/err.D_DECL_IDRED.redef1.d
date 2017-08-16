/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Test handling of an inline definition that overrides a previous
 * definition of an inline definition.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES:
 *
 */

inline int foo = timestamp;
inline int foo = 8;
