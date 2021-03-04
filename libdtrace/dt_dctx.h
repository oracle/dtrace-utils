/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _DT_DCTX_H
#define _DT_DCTX_H

#include <stdint.h>
#include <linux/bpf.h>
#include <bpf_asm.h>
#include <dt_pt_regs.h>
#include <dt_state.h>

/*
 * The DTrace machine state.
 */
typedef struct dt_mstate {
	uint32_t	epid;		/* Enabled probe ID */
	uint32_t	prid;		/* Probe ID */
	uint32_t	clid;		/* Clause ID (unique per probe) */
	uint32_t	tag;		/* Tag (for future use) */
	uint64_t	fault;		/* DTrace fault flags */
	uint64_t	tstamp;		/* cached timestamp value */
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
	dt_activity_t	*act;		/* pointer to activity state */
	dt_mstate_t	*mst;		/* DTrace machine state */
	char		*buf;		/* Output buffer scratch memory */
	char		*agg;		/* Aggregation data */
	char		*gvars;		/* Global variables */
	char		*lvars;		/* Local variables */
} dt_dctx_t;

#define DCTX_CTX	offsetof(dt_dctx_t, ctx)
#define DCTX_ACT	offsetof(dt_dctx_t, act)
#define DCTX_MST	offsetof(dt_dctx_t, mst)
#define DCTX_BUF	offsetof(dt_dctx_t, buf)
#define DCTX_AGG	offsetof(dt_dctx_t, agg)
#define DCTX_GVARS	offsetof(dt_dctx_t, gvars)
#define DCTX_LVARS	offsetof(dt_dctx_t, lvars)
#define DCTX_SIZE	((int16_t)sizeof(dt_dctx_t))

/*
 * Macro to determine the (negative) offset from the frame pointer (%fp) for
 * the given offset in dt_dctx_t.
 */
#define DCTX_FP(off)	(DT_STK_DCTX + (int16_t)(off))

#define DMST_EPID	offsetof(dt_mstate_t, epid)
#define DMST_PRID	offsetof(dt_mstate_t, prid)
#define DMST_CLID	offsetof(dt_mstate_t, clid)
#define DMST_TAG	offsetof(dt_mstate_t, tag)
#define DMST_FAULT	offsetof(dt_mstate_t, fault)
#define DMST_TSTAMP	offsetof(dt_mstate_t, tstamp)
#define DMST_REGS	offsetof(dt_mstate_t, regs)
#define DMST_ARG(n)	offsetof(dt_mstate_t, argv[n])

/*
 * DTrace BPF programs can use all BPF registers except for the the %fp (frame
 * pointer) register and the highest numbered register (currently %r9) that is
 * used to store the base pointer for the trace output record.
 */
#define DT_STK_NREGS		(MAX_BPF_REG - 2)

/*
 * The stack layout for functions that implement a D clause is encoded with the
 * following constants.
 *
 * Note: The BPF frame pointer points to the address of the first byte past the
 *       end of the stack.  8-byte values are properly aligned at offsets -8,
 *       -16, -24, -32, etc. -- that is, negative multiples of sizeof(uint64_t).
 *
 *                       +----------------+
 *          SCRATCH_BASE | Scratch Memory |
 *                       +----------------+
 *              SPILL(n) | %r8            | (n = DT_STK_NREGS - 1 = 8)
 *                       +----------------+
 *                       |      ...       |
 *                       +----------------+
 *              SPILL(1) | %r1            |
 *                       +----------------+
 * SPILL_BASE = SPILL(0) | %r0            |
 *                       +----------------+
 *                  DCTX | DTrace Context |
 *                       +----------------+
 */
#define DT_STK_BASE		((int16_t)0)
#define DT_STK_SLOT_SZ		((int16_t)sizeof(uint64_t))

#define DT_STK_DCTX		(DT_STK_BASE - DCTX_SIZE)
#define DT_STK_SPILL_BASE	(DT_STK_DCTX - DT_STK_SLOT_SZ)
#define DT_STK_SPILL(n)		(DT_STK_SPILL_BASE - (n) * DT_STK_SLOT_SZ)

#define DT_STK_SCRATCH_BASE	(-MAX_BPF_STACK)
#define DT_STK_SCRATCH_SZ	(MAX_BPF_STACK + DT_STK_SPILL(DT_STK_NREGS - 1))

#endif /* _DT_DCTX_H */
