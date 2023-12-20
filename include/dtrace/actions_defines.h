/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2022, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_ACTIONS_DEFINES_H
#define _DTRACE_ACTIONS_DEFINES_H

#include <sys/dtrace_types.h>
#include <dtrace/universal.h>

/*
 * The upper byte determines the class of the action; the low bytes determines
 * the specific action within that class.  The classes of actions are as
 * follows:
 *
 *   [ no class ]                  <= May record process- or kernel-related data
 *   DTRACEACT_PROC                <= Only records process-related data
 *   DTRACEACT_PROC_DESTRUCTIVE    <= Potentially destructive to processes
 *   DTRACEACT_KERNEL              <= Only records kernel-related data
 *   DTRACEACT_KERNEL_DESTRUCTIVE  <= Potentially destructive to the kernel
 *   DTRACEACT_SPECULATIVE         <= Speculation-related action
 *   DTRACEACT_AGGREGATION         <= Aggregating action
 */
#define	DTRACEACT_NONE			0	/* no action */
#define	DTRACEACT_DIFEXPR		1	/* action is DIF expression */
#define	DTRACEACT_EXIT			2	/* exit() action */
#define	DTRACEACT_PRINTF		3	/* printf() action */
#define	DTRACEACT_PRINTA		4	/* printa() action */
#define	DTRACEACT_LIBACT		5	/* library-controlled action */
#define	DTRACEACT_TRACEMEM		6	/* tracemem() action */
#define	DTRACEACT_PCAP			7	/* pcap() action */
#define	DTRACEACT_PRINT			8	/* print() action */

#define DTRACEACT_PROC			0x0100
#define DTRACEACT_USTACK		(DTRACEACT_PROC + 1)
#define DTRACEACT_JSTACK		(DTRACEACT_PROC + 2)
#define DTRACEACT_USYM			(DTRACEACT_PROC + 3)
#define DTRACEACT_UMOD			(DTRACEACT_PROC + 4)
#define DTRACEACT_UADDR			(DTRACEACT_PROC + 5)

#define DTRACEACT_PROC_DESTRUCTIVE	0x0200
#define DTRACEACT_STOP			(DTRACEACT_PROC_DESTRUCTIVE + 1)
#define DTRACEACT_RAISE			(DTRACEACT_PROC_DESTRUCTIVE + 2)
#define DTRACEACT_SYSTEM		(DTRACEACT_PROC_DESTRUCTIVE + 3)
#define DTRACEACT_FREOPEN		(DTRACEACT_PROC_DESTRUCTIVE + 4)

#define DTRACEACT_PROC_CONTROL		0x0300

#define DTRACEACT_KERNEL		0x0400
#define DTRACEACT_STACK			(DTRACEACT_KERNEL + 1)
#define DTRACEACT_SYM			(DTRACEACT_KERNEL + 2)
#define DTRACEACT_MOD			(DTRACEACT_KERNEL + 3)

#define DTRACEACT_KERNEL_DESTRUCTIVE	0x0500
#define DTRACEACT_BREAKPOINT		(DTRACEACT_KERNEL_DESTRUCTIVE + 1)
#define DTRACEACT_PANIC			(DTRACEACT_KERNEL_DESTRUCTIVE + 2)
#define DTRACEACT_CHILL			(DTRACEACT_KERNEL_DESTRUCTIVE + 3)

#define DTRACEACT_SPECULATIVE           0x0600
#define DTRACEACT_SPECULATE		(DTRACEACT_SPECULATIVE + 1)
#define DTRACEACT_COMMIT		(DTRACEACT_SPECULATIVE + 2)
#define DTRACEACT_DISCARD		(DTRACEACT_SPECULATIVE + 3)

#define DTRACEACT_CLASS(x)		((x) & 0xff00)

#define DTRACEACT_ISAGG(x)		\
		(DTRACEACT_CLASS(x) == DTRACEACT_AGGREGATION)

#define DTRACEACT_ISDESTRUCTIVE(x)	\
		(DTRACEACT_CLASS(x) == DTRACEACT_PROC_DESTRUCTIVE || \
		 DTRACEACT_CLASS(x) == DTRACEACT_KERNEL_DESTRUCTIVE)

#define DTRACEACT_ISSPECULATIVE(x)	\
		(DTRACEACT_CLASS(x) == DTRACEACT_SPECULATIVE)

#define DTRACEACT_ISPRINTFLIKE(x)	\
		((x) == DTRACEACT_PRINTF || (x) == DTRACEACT_PRINTA || \
		 (x) == DTRACEACT_SYSTEM || (x) == DTRACEACT_FREOPEN)

/*
 * DTrace Aggregating Actions
 *
 * These are functions f(x) for which the following is true:
 *
 *    f(f(x_0) U f(x_1) U ... U f(x_n)) = f(x_0 U x_1 U ... U x_n)
 *
 * where x_n is a set of arbitrary data.  Aggregating actions are in their own
 * DTrace action class, DTTRACEACT_AGGREGATION.  The macros provided here allow
 * for easier processing of the aggregation argument and data payload for a few
 * aggregating actions (notably:  quantize(), lquantize(), and ustack()).
 */

#define DTRACEACT_AGGREGATION		0x0700

#define DTRACE_QUANTIZE_NBUCKETS		\
		(((sizeof(uint64_t) * NBBY) - 1) * 2 + 1)

#define DTRACE_QUANTIZE_ZEROBUCKET	((sizeof(uint64_t) * NBBY) - 1)

#define DTRACE_QUANTIZE_BUCKETVAL(buck)		\
	(int64_t)((buck) < DTRACE_QUANTIZE_ZEROBUCKET ? \
		  -(1LL << (DTRACE_QUANTIZE_ZEROBUCKET - 1 - (buck))) : \
		  (buck) == DTRACE_QUANTIZE_ZEROBUCKET ? 0 : \
		  1LL << ((buck) - DTRACE_QUANTIZE_ZEROBUCKET - 1))

#define DTRACE_LQUANTIZE_STEPSHIFT	48
#define DTRACE_LQUANTIZE_STEPMASK	((uint64_t)UINT16_MAX << 48)
#define DTRACE_LQUANTIZE_LEVELSHIFT	32
#define DTRACE_LQUANTIZE_LEVELMASK	((uint64_t)UINT16_MAX << 32)
#define DTRACE_LQUANTIZE_BASESHIFT	0
#define DTRACE_LQUANTIZE_BASEMASK	UINT32_MAX

#define DTRACE_LQUANTIZE_STEP(x)		\
		(uint16_t)(((x) & DTRACE_LQUANTIZE_STEPMASK) >> \
			   DTRACE_LQUANTIZE_STEPSHIFT)

#define DTRACE_LQUANTIZE_LEVELS(x)		\
		(uint16_t)(((x) & DTRACE_LQUANTIZE_LEVELMASK) >> \
			   DTRACE_LQUANTIZE_LEVELSHIFT)

#define DTRACE_LQUANTIZE_BASE(x)		\
		(int32_t)(((x) & DTRACE_LQUANTIZE_BASEMASK) >> \
			  DTRACE_LQUANTIZE_BASESHIFT)

#define DTRACE_LLQUANTIZE_STEPSSHIFT	48
#define DTRACE_LLQUANTIZE_STEPSMASK	((uint64_t)UINT16_MAX << 48)
#define DTRACE_LLQUANTIZE_HMAGSHIFT	32
#define DTRACE_LLQUANTIZE_HMAGMASK	((uint64_t)UINT16_MAX << 32)
#define DTRACE_LLQUANTIZE_LMAGSHIFT	16
#define DTRACE_LLQUANTIZE_LMAGMASK	((uint64_t)UINT16_MAX << 16)
#define DTRACE_LLQUANTIZE_FACTORSHIFT	0
#define DTRACE_LLQUANTIZE_FACTORMASK	UINT16_MAX

#define DTRACE_LLQUANTIZE_STEPS(x)		\
		(uint16_t)(((x) & DTRACE_LLQUANTIZE_STEPSMASK) >> \
			DTRACE_LLQUANTIZE_STEPSSHIFT)

#define DTRACE_LLQUANTIZE_HMAG(x)		\
		(uint16_t)(((x) & DTRACE_LLQUANTIZE_HMAGMASK) >> \
			DTRACE_LLQUANTIZE_HMAGSHIFT)

#define DTRACE_LLQUANTIZE_LMAG(x)		\
		(uint16_t)(((x) & DTRACE_LLQUANTIZE_LMAGMASK) >> \
			DTRACE_LLQUANTIZE_LMAGSHIFT)

#define DTRACE_LLQUANTIZE_FACTOR(x)		\
		(uint16_t)(((x) & DTRACE_LLQUANTIZE_FACTORMASK) >> \
			DTRACE_LLQUANTIZE_FACTORSHIFT)

#define DTRACE_USTACK_NFRAMES(x)	(uint32_t)((x) & UINT32_MAX)
#define DTRACE_USTACK_STRSIZE(x)	(uint32_t)((x) >> 32)
#define DTRACE_USTACK_ARG(x, y)		\
		((((uint64_t)(y)) << 32) | ((x) & UINT32_MAX))

#ifndef _LP64
# ifndef _LITTLE_ENDIAN
#  define DTRACE_PTR(type, name)	uint32_t name##pad; type *name
# else
#  define DTRACE_PTR(type, name)	type *name; uint32_t name##pad
# endif
#else
# define DTRACE_PTR(type, name)		type *name
#endif

#endif /* _DTRACE_ACTIONS_DEFINES_H */
