/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: longsleep */

/*
 * ASSERTION: Only entry, return and offsets are valid names
 *
 * SECTION: User Process Tracing/pid Provider
 *
 */

pid$target:a.out:main:beginning
{
}
