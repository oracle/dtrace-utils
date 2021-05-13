/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-basic */

#pragma D option quiet
#pragma D option maxframes=95

profile-1
/pid == $target/
{
    ustack(25);
    exit(0);
}
