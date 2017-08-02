/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BUF_H
#define	_DT_BUF_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <dtrace.h>

typedef struct dt_buf {
	const char *dbu_name;	/* string name for debugging */
	uchar_t *dbu_buf;	/* buffer base address */
	uchar_t *dbu_ptr;	/* current buffer location */
	size_t dbu_len;		/* buffer size in bytes */
	int dbu_err;		/* errno value if error */
	int dbu_resizes;	/* number of resizes */
} dt_buf_t;

extern void dt_buf_create(dtrace_hdl_t *, dt_buf_t *, const char *, size_t);
extern void dt_buf_destroy(dtrace_hdl_t *, dt_buf_t *);
extern void dt_buf_reset(dtrace_hdl_t *, dt_buf_t *);

extern void dt_buf_write(dtrace_hdl_t *, dt_buf_t *,
    const void *, size_t, size_t);

extern void dt_buf_concat(dtrace_hdl_t *, dt_buf_t *,
    const dt_buf_t *, size_t);

extern size_t dt_buf_offset(const dt_buf_t *, size_t);
extern size_t dt_buf_len(const dt_buf_t *);

extern int dt_buf_error(const dt_buf_t *);
extern void *dt_buf_ptr(const dt_buf_t *);

extern void *dt_buf_claim(dtrace_hdl_t *, dt_buf_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BUF_H */
