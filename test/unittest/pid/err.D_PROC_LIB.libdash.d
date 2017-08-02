/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@no-xfail */

/*
 * ASSERTION: Test that the '-' function doesn't work with random modules
 *
 * SECTION: User Process Tracing/pid Provider
 *
 */

pid$target:libc:-:800
{
}
