#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: readwholedir */

syscall::write:entry
/pid == $target/
{
}

syscall::exit_group:entry
/pid == $target/
{
       exit(0);
}

