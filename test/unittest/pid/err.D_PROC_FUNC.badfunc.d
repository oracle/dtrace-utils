/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@no-xfail */

/*
 * ASSERTION: Can't specify a bogus function name (if the module is specified)
 *
 * SECTION: User Process Tracing/pid Provider
 *
 * NOTES:
 *
 */

pid$target:a.out:ahl_r00lz:entry
{
}
