/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *
 * Test an invalid inline definition of a translator.  An inlined translation
 * must have the same type as the translator output.
 *
 * SECTION: Type and Constant Definitions/Inlines
 *
 * NOTES:
 *
 */

inline vfs_t *invalid = xlate<psinfo_t>(curthread->t_procp);
