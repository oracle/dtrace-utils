/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C */

/*
 * ASSERTION:
 *   Declare a dynamic type and then use it to access the root path for the
 *   current process.
 *
 * SECTION: Structs and Unions/Structs;
 *	Variables/External Variables
 *
 * NOTES:
 *  This test program declares a dynamic type and then uses it to bcopy() the
 *  qstr representing the name of the root dentry for the current process.  We
 *  then use the dynamic type to access the result of our bcopy().  The
 *  special "D" module type scope is also tested.
 */

#pragma D option quiet

#include <endian.h>

struct dqstr {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int hash;
	unsigned int len;
#else
	unsigned int len;
	unsigned int hash;
#endif
	unsigned char *name;
};

BEGIN
{
	s = (struct D`dqstr *)alloca(sizeof(struct D`dqstr));
	bcopy(&curthread->fs->root.dentry->d_name, s, sizeof (struct D`dqstr));

	printf("hash = %d\n", s->hash);
	printf("len = %d\n", s->len);
	printf("name = \"%s\"\n", stringof(s->name));

	exit(0);
}
