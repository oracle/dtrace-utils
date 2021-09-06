/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PEB_H
#define	_DT_PEB_H

#include <stddef.h>
#include <stdint.h>

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Perf event buffer.
 */
typedef struct dt_peb {
	dtrace_hdl_t	*dtp;		/* pointer to containing dtrace_hdl */
	int		cpu;		/* ID of CPU that uses this buffer */
	int		fd;		/* fd of perf output buffer */
	char		*base;		/* address of buffer */
	char		*endp;		/* address of end of buffer */
	uint64_t	last_head;	/* last known head, for peeking */
} dt_peb_t;

/*
 * Set of perf event buffers.  This structure stores buffer information that is
 * shared between all buffers, a shared chunk of memory to copy any event that
 * spans the ring buffer boundary, and an array of perf event buffers.
 */
typedef struct dt_pebset {
	size_t		page_size;	/* size of each page in buffer */
	size_t		data_size;	/* total buffer size */
	struct dt_peb	*pebs;		/* array of perf event buffers */
	char		*tmp;		/* temporary event buffer */
	size_t		tmp_len;	/* length of temporary event buffer */
} dt_pebset_t;

extern void dt_pebs_exit(dtrace_hdl_t *);
extern int dt_pebs_init(dtrace_hdl_t *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PEB_H */
