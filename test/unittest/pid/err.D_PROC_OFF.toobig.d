/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */
/* @@no-xfail */

/*
 * ASSERTION: Can't have an offset that's outside of a function
 *
 * SECTION: User Process Tracing/pid Provider
 *
 * NOTES: If _exit(2) becomes _way_ more complex this could fail...
 *
 */

pid$target::_exit:100
{
}
