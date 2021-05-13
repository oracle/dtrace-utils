/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-basic */

#pragma D option quiet

pid$target:a.out:myfunc_z:entry
{
    ustack(25);
    exit(0);
}
