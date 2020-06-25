/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _DT_DCTX_H
#define _DT_DCTX_H

#include <dt_pt_regs.h>

/*
 * The DTrace machien state.
 */
typedef struct dt_mstate {
	uint32_t	epid;		/* Enabled probe ID */
	uint32_t	tag;		/* Tag (for future use) */
	uint64_t	fault;		/* DTrace fault flags */
#if 0
	dt_pt_regs	regs;		/* CPU registers */
#endif
	uint64_t	argv[10];	/* Probe arguments */
} dt_mstate_t;

/*
 * The DTrace context.
 */
typedef struct dt_dctx {
	void		*ctx;		/* BPF context */
	dt_mstate_t	*mst;		/* DTrace machine state */
	char		*buf;		/* Output buffer scratch memory */
} dt_dctx_t;

#define DCTX_CTX	offsetof(dt_dctx_t, ctx)
#define DCTX_MST	offsetof(dt_dctx_t, mst)
#define DCTX_BUF	offsetof(dt_dctx_t, buf)
#define DCTX_SIZE	sizeof(dt_dctx_t)

/*
 * Macro to determine the (negative) offset from the frame pointer (%fp) for
 * the given offset in dt_dctx_t.
 */
#define DCTX_FP(off)	(-(ushort_t)DCTX_SIZE + (ushort_t)(off))

#define DMST_EPID	offsetof(dt_mstate_t, epid)
#define DMST_TAG	offsetof(dt_mstate_t, tag)
#define DMST_FAULT	offsetof(dt_mstate_t, fault)
#define DMST_REGS	offsetof(dt_mstate_t, regs)
#define DMST_ARG(n)	offsetof(dt_mstate_t, argv[n])

#endif /* _DT_DCTX_H */
