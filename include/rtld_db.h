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
 * Copyright 2008 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#ifndef	_RTLD_DB_H
#define	_RTLD_DB_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/link.h>

/*
 * iteration over load objects
 */
typedef struct rd_loadobj {
	psaddr_t	rl_nameaddr;	/* address of the name in user space */
	unsigned	rl_flags;
	psaddr_t	rl_base;	/* base of address of code */
	psaddr_t	rl_data_base;	/* base of address of data */
	Lmid_t		rl_lmident;	/* ident of link map */
	psaddr_t	rl_refnameaddr;	/* reference name of filter in user */
					/* space.  If non null object is a */
					/* filter. */
	psaddr_t	rl_plt_base;	/* These fields are present for 4.x */
	unsigned	rl_plt_size;	/* compatibility and are not */
					/* currently used  in SunOS5.x */
	psaddr_t	rl_bend;	/* end of image (text+data+bss) */
	psaddr_t	rl_padstart;	/* start of padding */
	psaddr_t	rl_padend;	/* end of image after padding */
	psaddr_t	rl_dynamic;	/* points to the DYNAMIC section */
					/* in the target process */
	unsigned long	rl_tlsmodid;	/* module ID for TLS references */
} rd_loadobj_t;

typedef struct rd_agent rd_agent_t;
#ifdef __STDC__
typedef int rl_iter_f(const rd_loadobj_t *, void *);
#else
typedef int rl_iter_f();
#endif



#ifdef	__cplusplus
}
#endif

#endif	/* _RTLD_DB_H */
