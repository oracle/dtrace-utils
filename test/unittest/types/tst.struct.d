/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

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

struct dqstr {
	unsigned int hash;
	unsigned int len;
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
