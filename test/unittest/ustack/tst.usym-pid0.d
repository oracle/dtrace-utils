/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* Without a trigger, pid will be 0, but we should not segfault. */

#pragma D option quiet

profile-100ms
/pid == $target/
{
    usym(ucaller);
    printf("\n");
    exit(0);
}
