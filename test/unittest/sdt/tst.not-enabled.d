/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */

sdt:dt_test::sdt-test-arg1 {}
sdt:dt_test::sdt-test-is-enabled { exit(1); }

syscall::exit_group:entry
/execname == "testprobe"/
{
        exit(0);
}
