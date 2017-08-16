/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the different kinds of integer scalar references.  In particular, we
 *  test accessing a kernel executable scalar, kernel scoped scalar, module
 *  scalar, kernel statically-scoped scalar, DTrace scalar first ref that
 *  forces creation (both global and TLS), and DTrace scalar subsequent
 *  reference (both global and TLS).
 *
 * SECTION:  Variables/External Variables
 *
 */

BEGIN
{
	printf("\nr_cpu_ids = 0x%x\n", `nr_cpu_ids);
	printf("ext2`ext2_dir_operations = %p\n", &ext2`ext2_dir_operations);
	printf("isofs`isofs_dir_operations = %p\n", &isofs`isofs_dir_operations);
	printf("vmlinux`major_names = %p\n", &vmlinux`major_names);
	x = 123;
	printf("x = %u\n", x);
	self->x = 456;
	printf("self->x = %u\n", self->x);
	exit(0);
}
