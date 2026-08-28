/*
 * Oracle Linux DTrace; USDT definitions parser.
 * Copyright (c) 2010, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/compiler.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libelf.h>
#include "usdt_parser.h"

size_t			usdt_maxcount = 2;

_dt_printflike_(3, 4)
void
usdt_error(int out, int err_no, const char *fmt, ...)
{
	dof_parsed_t *parsed;
	size_t sz;
	char *msg;
	va_list ap;

	/*
	 * Not much we can do on OOM of errors other than abort, forcing a
	 * parser restart, which hopefully will have enough memory to report the
	 * error properly.
	 */
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) < 0)
		abort();
	va_end(ap);

	sz = offsetof(dof_parsed_t, err.err) + strlen(msg) + 1;
	parsed = malloc(sz);

	if (!parsed)
		abort();

	memset(parsed, 0, sz);
	parsed->size = sz;
	parsed->type = DIT_ERR;
	parsed->err.err_no = err_no;
	strcpy(parsed->err.err, msg);

	usdt_parser_write_one(out, parsed, parsed->size);
	free(parsed);
	free(msg);
}

static char *
usdt_copyin(int in, char *buf_, size_t sz)
{
	char *buf = buf_;
	size_t i;

	if (!buf) {
		buf = malloc(sz);
		if (!buf)
			abort();
	}

	memset(buf, 0, sz);

	for (i = 0; i < sz; ) {
		size_t ret;

		ret = read(in, buf + i, sz - i);

		if (ret < 0) {
			switch (errno) {
			case EINTR:
				continue;
			default:
				goto err;
			}
		}

		/*
		 * EOF: parsing done, process shutting down or message
		 * truncated.  Fail, in any case.
		 */
		if (ret == 0)
			goto err;

		i += ret;
	}

	return buf;

err:
	if (!buf_)
		free(buf);
	return NULL;
}

dof_helper_t *
usdt_copyin_helper(int in)
{
	return (dof_helper_t *)usdt_copyin(in, NULL, sizeof(dof_helper_t));
}

static usdt_data_t *
usdt_copyin_block(int in, int out, int *ok)
{
	usdt_data_t *data;

	*ok = 1;

	data = malloc(sizeof(usdt_data_t));
	if (!data)
		abort();

	memset(data, 0, sizeof(usdt_data_t));

	/* Get the offset of the data block. */
	if (!usdt_copyin(in, (char *)&data->base, sizeof(data->base)))
		abort();

	/* Get the size of the data block. */
	if (!usdt_copyin(in, (char *)&data->size, sizeof(data->size)))
		abort();

	/* Validate the data size. */
	if (data->size > DOF_MAXSZ) {
		usdt_error(out, E2BIG, "data size %zi exceeds maximum %i",
			   data->size, DOF_MAXSZ);
		return NULL;
	}

	/* Get the data. */
	data->buf = (void *)usdt_copyin(in, NULL, data->size);
	if (!data->buf) {
		*ok = 0;
		free(data);
		return NULL;
	}

	return data;
}

static void
usdt_destroy_data(usdt_data_t *data)
{
	while (data) {
		usdt_data_t *next = data->next;

		free(data->buf);
		free(data);
		data = next;
	}
}

usdt_data_t *
usdt_copyin_data(int in, int out, int *ok)
{
	usdt_data_t *first = NULL, *last;
	size_t cnt;

	*ok = 1;

	/* Get the number of data blocks to follow. */
	if (!usdt_copyin(in, (char *)&cnt, sizeof(cnt)))
		abort();

	if (cnt > usdt_maxcount) {
		usdt_error(out, E2BIG, "block count %zi exceeds maximum %zi",
			   cnt, usdt_maxcount);
		return NULL;
	}

	/* Get the data blocks (for each, size followed by content). */
	while (cnt-- > 0) {
		usdt_data_t *blk;

		if ((blk = usdt_copyin_block(in, out, ok)) == NULL)
			goto err;

		if (first == NULL)
			first = last = blk;
		else {
			last->next = blk;
			last = blk;
		}
	}

	return first;

err:
	usdt_destroy_data(first);

	return NULL;
}

static void
usdt_destroy(dof_helper_t *dhp, usdt_data_t *data)
{
	free(dhp);
	usdt_destroy_data(data);
}

void
usdt_parse(int out, dof_helper_t *dhp, usdt_data_t *data)
{
	dof_parsed_t	eof;
	int 		rc = -1;

	if (dhp->dofhp_dof)
		rc = usdt_parse_dof(out, dhp, data->buf);
	else
		rc = usdt_parse_notes(out, dhp, data);
	if (rc != 0)
		goto err;

	/*
	 * Always emit an EOF, to wake up the caller if nothing else, but also
	 * to notify the caller that there are no more providers to read.
	 */
	memset(&eof, 0, sizeof(dof_parsed_t));

	eof.size = offsetof(dof_parsed_t, provider.nprobes);
	eof.type = DIT_EOF;
	usdt_parser_write_one(out, &eof, eof.size);

err:
	usdt_destroy(dhp, data);
}
