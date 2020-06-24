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
 * The DTrace context.
 */
typedef struct dt_dctx {
	uint32_t	epid;
	uint32_t	pad;
	uint64_t	fault;
#if 0
	dt_pt_regs	regs;
#endif
	uint64_t	argv[10];
} dt_dctx_t;

#define DCTX_EPID	offsetof(dt_dctx_t, epid)
#define DCTX_PAD	offsetof(dt_dctx_t, pad)
#define DCTX_FAULT	offsetof(dt_dctx_t, fault)
#define DCTX_REGS	offsetof(dt_dctx_t, regs)
#define DCTX_ARG(n)	offsetof(dt_dctx_t, argv[n])
#define DCTX_SIZE	sizeof(dt_dctx_t)

#endif /* _DT_DCTX_H */
