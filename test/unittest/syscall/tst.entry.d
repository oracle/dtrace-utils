/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: syscall-tst-args */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/* check that syscall:::entry picks up mmap:entry */
/* (mmap is called repeatedly by the trigger) */
/* trigger-timing and runtest-opts are used since -c is not yet supported */

syscall:::entry
/pid == $1/
{
    exit(0);
}
