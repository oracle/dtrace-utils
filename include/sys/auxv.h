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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright 2008 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_AUXV_H
#define	_SYS_AUXV_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"	/* SVr4.0 1.2	*/

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct
{
	int	a_type;
	union {
		long	a_val;
#ifdef __STDC__
		void	*a_ptr;
#else
		char	*a_ptr;
#endif
		void	(*a_fcn)();
	} a_un;
} auxv_t;


#ifdef	__cplusplus
}
#endif
#endif	/* _SYS_AUXV_H */
