/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
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

typedef struct dqstr {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int hash;
	unsigned int len;
#else
	unsigned int len;
	unsigned int hash;
#endif
	unsigned char *name;
} dqstr_t;

BEGIN
{
	s = (D`dqstr_t *)alloca(sizeof(D`dqstr_t));
	bcopy(&curthread->fs->root.dentry->d_name, s, sizeof(D`dqstr_t));

	printf("hash = %d\n", s->hash);
	printf("len = %d\n", s->len);
	printf("name = \"%s\"\n", stringof(s->name));

	exit(0);
}
