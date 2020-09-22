/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
} dt_dctx_t;

#define DCTX_CTX	offsetof(dt_dctx_t, ctx)
#define DCTX_ACT	offsetof(dt_dctx_t, act)
#define DCTX_MST	offsetof(dt_dctx_t, mst)
#define DCTX_BUF	offsetof(dt_dctx_t, buf)
#define DCTX_AGG	offsetof(dt_dctx_t, agg)
#define DCTX_SIZE	((int16_t)sizeof(dt_dctx_t))

/*
 * Macro to determine the (negative) offset from the frame pointer (%fp) for
 * the given offset in dt_dctx_t.
 */
#define DCTX_FP(off)	(DT_STK_DCTX + (int16_t)(off))

#define DMST_EPID	offsetof(dt_mstate_t, epid)
#define DMST_PRID	offsetof(dt_mstate_t, prid)
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
 * An area of 256 bytes is set aside on the stack as scratch area for things
 * like string operations.  It extends from the end of the stack towards the
 * local variable storage area.  DT_STK_LVAR_END is therefore the last byte
 * location on the stack that can be used for local variable storage.
 */
#define DT_STK_SCRATCH_BASE	(-MAX_BPF_STACK)
#define DT_STK_SCRATCH_SZ	(256)
#define DT_STK_LVAR_END		(DT_STK_SCRATCH_BASE + DT_STK_SCRATCH_SZ)

/*
 * The stack layout for functions that implement a D clause is encoded with the
 * following constants.
 *
 * Note: The BPF frame pointer points to the address of the first byte past the
 *       end of the stack.  If the stack size is 512 bytes, valid offsets are
 *       -1 through -512 (inclusive),  So, the first 64-bit value on the stack
 *       occupies bytes at offsets -8 through -1.  The second -16 through -9,
 *       and so on...  64-bit values are properly aligned at offsets -n where
 *       n is a multiple of 8 (sizeof(uint64_t)).
 *
 * The following diagram shows the stack layout for a size of 512 bytes.
 *
 *                             +----------------+
 *         SCRATCH_BASE = -512 | Scratch Memory |
 *                             +----------------+
 *   LVAR_END = LVAR(n) = -256 | LVAR n         | (n = DT_LVAR_MAX = 17)
 *                             +----------------+
 *                             |      ...       |
 *                             +----------------+
 *              LVAR(1) = -128 | LVAR 1         |
 *                             +----------------+
 *  LVAR_BASE = LVAR(0) = -120 | LVAR 0         |
 *                             +----------------+
 *             SPILL(n) = -112 | %r8            | (n = DT_STK_NREGS - 1 = 8)
 *                             +----------------+
 *                             |      ...       |
 *                             +----------------+
 *              SPILL(1) = -56 | %r1            |
 *                             +----------------+
 * SPILL_BASE = SPILL(0) = -48 | %r0            |
 *                             +----------------+
 *                  DCTX = -40 | DTrace Context | -1
 *                             +----------------+
 */
#define DT_STK_BASE		((int16_t)0)
#define DT_STK_SLOT_SZ		((int16_t)sizeof(uint64_t))

#define DT_STK_DCTX		(DT_STK_BASE - DCTX_SIZE)
#define DT_STK_SPILL_BASE	(DT_STK_DCTX - DT_STK_SLOT_SZ)
#define DT_STK_SPILL(n)		(DT_STK_SPILL_BASE - (n) * DT_STK_SLOT_SZ)
#define DT_STK_LVAR_BASE	(DT_STK_SPILL(DT_STK_NREGS - 1) - \
				 DT_STK_SLOT_SZ)
#define DT_STK_LVAR(n)		(DT_STK_LVAR_BASE - (n) * DT_STK_SLOT_SZ)

/*
 * Calculate a local variable ID based on a given stack offset.  If the stack
 * offset is outside the valid range, this should evaluate as -1.
 */
#define DT_LVAR_OFF2ID(n)	(((n) > DT_STK_LVAR_BASE || \
				  (n) < DT_STK_LVAR_END) ? -1 : \
				 (-((n) - DT_STK_LVAR_BASE) / DT_STK_SLOT_SZ))

/*
 * Maximum number of local variables stored by value (scalars).  This is bound
 * by the choice to store them on the stack between the register spill space,
 * and 256 bytes set aside as string scratch space.  We also use the fact that
 * the (current) maximum stack space for BPF programs is 512 bytes.
 */
#define DT_LVAR_MAX		(-(DT_STK_LVAR_END - DT_STK_LVAR_BASE) / \
				 DT_STK_SLOT_SZ)

#endif /* _DT_DCTX_H */
