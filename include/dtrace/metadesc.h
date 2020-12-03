/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2020, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Note: The contents of this file are private to the implementation of the
 * DTrace subsystem and are subject to change at any time without notice.
 */

#ifndef _DTRACE_METADESC_H
#define _DTRACE_METADESC_H

#include <dtrace/universal.h>
#include <dtrace/actions_defines.h>
#include <dtrace/metadesc_defines.h>

/*
 * DTrace separates the trace data stream from the metadata stream.  The only
 * metadata tokens placed in the data stream are enabled probe identifiers
 * (EPIDs) or (in the case of aggregations) aggregation identifiers.  In order
 * to determine the structure of the data, DTrace uses the token to perform a
 * lookup to retrieve the corresponding description of the enabled probe (via
 * the dtrace_datadesc structure) or the aggregation (via the dtrace_aggdesc
 * structure).
 *
 * Both of these structures are expressed in terms of record descriptions (via
 * the dtrace_recdesc structure) that describe the exact structure of the data.
 * Some record descriptions may also contain a format identifier; this
 * additional bit of metadata refers to a format description described via a
 * dtrace_fmtdesc structure.
 */
typedef struct dtrace_recdesc {
	dtrace_actkind_t dtrd_action;		/* kind of action */
	uint32_t dtrd_size;			/* size of record */
	uint32_t dtrd_offset;			/* offset in ECB's data */
	uint16_t dtrd_alignment;		/* required alignment */
	void *dtrd_format;			/* format, if any */
	uint64_t dtrd_arg;			/* action argument */
	uint64_t dtrd_uarg;			/* user argument */
} dtrace_recdesc_t;

typedef struct dtrace_datadesc {
	uint64_t dtdd_uarg;			/* library argument */
	uint32_t dtdd_size;			/* total size */
	int dtdd_nrecs;				/* number of records */
	dtrace_recdesc_t *dtdd_recs;		/* records themselves */
	int dtdd_refcnt;			/* reference count */
} dtrace_datadesc_t;

typedef struct dtrace_aggdesc {
	DTRACE_PTR(char, dtagd_name);		/* not filled in by kernel */
	dtrace_aggid_t dtagd_varid;		/* not filled in by kernel */
	int dtagd_flags;			/* not filled in by kernel */
	dtrace_aggid_t dtagd_id;		/* aggregation ID */
	uint64_t dtagd_sig;			/* aggregation signature */
	uint32_t dtagd_size;			/* size in bytes */
	int dtagd_nrecs;			/* number of records */
	dtrace_recdesc_t *dtagd_recs;		/* record descriptions */
} dtrace_aggdesc_t;

typedef struct dtrace_fmtdesc {
	DTRACE_PTR(char, dtfd_string);		/* format string */
	int dtfd_length;			/* length of format string */
	uint16_t dtfd_format;			/* format identifier */
} dtrace_fmtdesc_t;

#define DTRACE_SIZEOF_EPROBEDESC(desc)				\
	(sizeof (dtrace_eprobedesc_t) + ((desc)->dtdd_nrecs ?  \
	(((desc)->dtdd_nrecs - 1) * sizeof (dtrace_recdesc_t)) : 0))

#define	DTRACE_SIZEOF_AGGDESC(desc)			       \
	(sizeof (dtrace_aggdesc_t) + ((desc)->dtagd_nrecs ?     \
	(((desc)->dtagd_nrecs - 1) * sizeof (dtrace_recdesc_t)) : 0))

#endif /* _DTRACE_METADESC_H */
