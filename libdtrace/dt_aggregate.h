/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_AGGREGATE_H
#define	_DT_AGGREGATE_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_aggregate dt_aggregate_t;

#define DT_AGGDATA_COUNTER	0
#define DT_AGGDATA_RECORD	1

/*
 * Aggregation functions.
 */
#define	DT_AGG_BASE		DTRACEACT_AGGREGATION
#define	DT_AGG(n)		(DT_AGG_BASE + (n))
#define DT_AGG_IDX(n)		((n) - DT_AGG_BASE)
#define DT_AGG_NUM		(DT_AGG_IDX(DT_AGG_HIGHEST))

typedef enum dt_aggfid {
	DT_AGG_AVG = DT_AGG_BASE,
	DT_AGG_COUNT,
	DT_AGG_LLQUANTIZE,
	DT_AGG_LQUANTIZE,
	DT_AGG_MAX,
	DT_AGG_MIN,
	DT_AGG_QUANTIZE,
	DT_AGG_STDDEV,
	DT_AGG_SUM,

	DT_AGG_HIGHEST
} dt_aggfid_t;

extern int dt_aggregate_init(dtrace_hdl_t *);
extern void dt_aggregate_set_option(dtrace_hdl_t *, uintptr_t);
extern int dt_aggregate_go(dtrace_hdl_t *);
extern int dt_aggregate_clear_one(const dtrace_aggdata_t *, void *);
extern void dt_aggregate_destroy(dtrace_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_AGGREGATE_H */
