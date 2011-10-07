/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_PUTIL_H
#define	_PUTIL_H

#include <sys/compiler.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Circular doubly-linked list:
 */
typedef struct P_list {
	struct P_list	*list_forw;
	struct P_list	*list_back;
} plist_t;

/*
 * Routines to manipulate linked lists:
 */
extern void list_link(void *, void *);
extern void list_unlink(void *);

#define	list_next(elem)	(void *)(((plist_t *)(elem))->list_forw)
#define	list_prev(elem)	(void *)(((plist_t *)(elem))->list_back)

/*
 * Routine to print debug messages:
 */
_dt_printflike_(1,2)
extern void _dprintf(const char *, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _PUTIL_H */
