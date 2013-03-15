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
 * Copyright 2008, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#ifndef	_RTLD_DB_H
#define	_RTLD_DB_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <link.h>

struct ps_prochandle;

/*
 * Errors.
 */

typedef enum {
	RD_ERR,		/* generic */
	RD_OK,		/* generic "call" succeeded */
	RD_NOMAPS	/* link-maps are not yet available */
} rd_err_e;

/*
 * Debugging events inside the runtime linker.
 */

typedef enum {
	RD_NONE = 0,		/* no event */
	RD_PREINIT,		/* the Initial rendezvous before .init: not yet
				   implemented */
	RD_POSTINIT,		/* the Second rendezvous after .init: not yet
				   implemented */
	RD_DLACTIVITY		/* a dlopen or dlclose has happened */
} rd_event_e;

/*
 * Information about event instance.  Identical to the r_state enumeration in
 * <link.h>, save for slightly different enumeration names.  If this ever
 * changes (very unlikely), we will need to adapt, or add a translation layer.
 */
typedef enum {
	RD_CONSISTENT,		/* link-maps are stable */
	RD_ADD,			/* currently adding object to link-maps */
	RD_DELETE		/* currently deleting object from link-maps */
} rd_state_e;

typedef struct rd_event_msg {
	rd_event_e	type;
	rd_state_e	state;	/* for DLACTIVITY */
} rd_event_msg_t;

struct rd_agent;
typedef struct rd_agent rd_agent_t;
/*
 * Called with rd_event_msg_t of NULL when deallocating state.
 */
typedef void (*rd_event_fun)(rd_agent_t *, rd_event_msg_t *, void *);

/*
 * iteration over load objects
 */
typedef struct rd_loadobj {
	intptr_t	rl_diff_addr;	/* Difference between addresses in ELF
					   file and addresses in memory */
	uintptr_t	rl_nameaddr;	/* address of the name in user space */
	uintptr_t	rl_dyn;		/* dynamic section of object */
	Lmid_t		rl_lmident;	/* ident of link map */
} rd_loadobj_t;

typedef int rl_iter_f(const rd_loadobj_t *, size_t, void *);

extern void		rd_delete(rd_agent_t *);
extern rd_err_e		rd_event_enable(rd_agent_t *, rd_event_fun fun, void *data);
extern void		rd_event_disable(rd_agent_t *rd);
extern rd_err_e		rd_loadobj_iter(rd_agent_t *, rl_iter_f *,
				void *);
extern rd_agent_t	*rd_new(struct ps_prochandle *);

#ifdef	__cplusplus
}
#endif

#endif	/* _RTLD_DB_H */
