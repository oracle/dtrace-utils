/*
 * Oracle Linux DTrace.
 * Copyright © 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */

sdt:dt_test::sdt-test-ioctl-file {}

sdt:dt_test::sdt-test-ioctl-file
{
        /* This lies beyond an elided probe argument. */
	trace(args[1]->f_cred->uid.val);
}

syscall::exit_group:entry
/execname == "testprobe"/
{
        exit(0);
}
