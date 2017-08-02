/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * DTrace Memory Buffer Routines
 *
 * The routines in this file are used to create an automatically resizing
 * memory buffer that can be written to like a file.  Memory buffers are
 * used to construct DOF to ioctl() to dtrace(7D), and provide semantics that
 * simplify caller code.  Specifically, any allocation errors result in an
 * error code being set inside the buffer which is maintained persistently and
 * propagates to another buffer if the buffer in error is concatenated.  These
 * semantics permit callers to execute a large series of writes without needing
 * to check for errors and then perform a single check before using the buffer.
 */

#include <strings.h>

#include <dt_impl.h>
#include <dt_buf.h>

void
dt_buf_create(dtrace_hdl_t *dtp, dt_buf_t *bp, const char *name, size_t len)
{
	if (len == 0)
		len = _dtrace_bufsize;

	bp->dbu_buf = bp->dbu_ptr = dt_zalloc(dtp, len);
	bp->dbu_len = len;

	if (bp->dbu_buf == NULL)
		bp->dbu_err = dtrace_errno(dtp);
	else
		bp->dbu_err = 0;

	bp->dbu_resizes = 0;
	bp->dbu_name = name;
}

void
dt_buf_destroy(dtrace_hdl_t *dtp, dt_buf_t *bp)
{
	dt_dprintf("dt_buf_destroy(%s): size=%lu resizes=%u\n",
	    bp->dbu_name, (ulong_t)bp->dbu_len, bp->dbu_resizes);

	dt_free(dtp, bp->dbu_buf);
}

void
dt_buf_reset(dtrace_hdl_t *dtp, dt_buf_t *bp)
{
	if ((bp->dbu_ptr = bp->dbu_buf) != NULL)
		bp->dbu_err = 0;
	else
		dt_buf_create(dtp, bp, bp->dbu_name, bp->dbu_len);
}

void
dt_buf_write(dtrace_hdl_t *dtp, dt_buf_t *bp,
    const void *buf, size_t len, size_t align)
{
	size_t off = (size_t)(bp->dbu_ptr - bp->dbu_buf);
	size_t adj = roundup(off, align) - off;

	if (bp->dbu_err != 0) {
		(void) dt_set_errno(dtp, bp->dbu_err);
		return; /* write silently fails */
	}

	if (bp->dbu_ptr + adj + len > bp->dbu_buf + bp->dbu_len) {
		size_t new_len = bp->dbu_len * 2;
		uchar_t *new_buf;
		uint_t r = 1;

		while (bp->dbu_ptr + adj + len > bp->dbu_buf + new_len) {
			new_len *= 2;
			r++;
		}

		if ((new_buf = dt_zalloc(dtp, new_len)) == NULL) {
			bp->dbu_err = dtrace_errno(dtp);
			return;
		}

		bcopy(bp->dbu_buf, new_buf, off);
		dt_free(dtp, bp->dbu_buf);

		bp->dbu_buf = new_buf;
		bp->dbu_ptr = new_buf + off;
		bp->dbu_len = new_len;
		bp->dbu_resizes += r;
	}

	bp->dbu_ptr += adj;
	bcopy(buf, bp->dbu_ptr, len);
	bp->dbu_ptr += len;
}

void
dt_buf_concat(dtrace_hdl_t *dtp, dt_buf_t *dst,
    const dt_buf_t *src, size_t align)
{
	if (dst->dbu_err == 0 && src->dbu_err != 0) {
		(void) dt_set_errno(dtp, src->dbu_err);
		dst->dbu_err = src->dbu_err;
	} else {
		dt_buf_write(dtp, dst, src->dbu_buf,
		    (size_t)(src->dbu_ptr - src->dbu_buf), align);
	}
}

size_t
dt_buf_offset(const dt_buf_t *bp, size_t align)
{
	size_t off = (size_t)(bp->dbu_ptr - bp->dbu_buf);
	return (roundup(off, align));
}

size_t
dt_buf_len(const dt_buf_t *bp)
{
	return (bp->dbu_ptr - bp->dbu_buf);
}

int
dt_buf_error(const dt_buf_t *bp)
{
	return (bp->dbu_err);
}

void *
dt_buf_ptr(const dt_buf_t *bp)
{
	return (bp->dbu_buf);
}

void *
dt_buf_claim(dtrace_hdl_t *dtp, dt_buf_t *bp)
{
	void *buf = bp->dbu_buf;

	if (bp->dbu_err != 0) {
		dt_free(dtp, buf);
		buf = NULL;
	}

	bp->dbu_buf = bp->dbu_ptr = NULL;
	bp->dbu_len = 0;

	return (buf);
}
