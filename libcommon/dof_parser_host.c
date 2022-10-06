/*
 * Oracle Linux DTrace; DOF-consumption and USDT-probe-creation daemon.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dof_parser.h"

/*
 * Write BUF to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int
dof_parser_write_one(int out, const void *buf_, size_t size)
{
	size_t i;
	char *buf = (char *) buf_;

	for (i = 0; i < size; ) {
		size_t ret;

		ret = write(out, buf + i, size - i);
		if (ret < 0) {
			switch (errno) {
			case EINTR:
				continue;
			default:
				return errno;
			}
		}

		i += ret;
	}

	return 0;
}

/*
 * Write the DOF to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int
dof_parser_host_write(int out, dof_helper_t *dh, dof_hdr_t *dof)
{
	int err;

	if ((err = dof_parser_write_one(out, (char *)dh,
					sizeof(dof_helper_t))) < 0)
		return err;

	return dof_parser_write_one(out, (char *)dof,
				    dof->dofh_loadsz);
}

/*
 * Read a single DOF structure from a parser pipe.  Wait at most TIMEOUT seconds
 * to do so.
 *
 * Returns NULL and sets errno on error.
 */
dof_parsed_t *
dof_parser_host_read(int in, int timeout)
{
	size_t i, sz;
	dof_parsed_t *reply;
	struct pollfd fd;

	fd.fd = in;
	fd.events = POLLIN;

	reply = malloc(sizeof(dof_parsed_t));
	if (!reply)
		goto err;
	memset(reply, 0, sizeof(dof_parsed_t));

	/*
	 * On the first read, only read in the size.  Decide how much to read
	 * only after that, both to make sure we don't underread and to make
	 * sure we don't *overread* and concatenate part of another message
	 * onto this one.
	 */
	for (i = 0, sz = offsetof(dof_parsed_t, type); i < sz;) {
		size_t ret;

		if ((ret = poll(&fd, 1, timeout * 1000)) <= 0)
			goto err;

		ret = read(in, ((char *) reply) + i, sz - i);

		if (ret <= 0)
			goto err;

		/*
		 * Fix up the size once it's received.  Might be large enough
		 * that we've done the initial size read...
		 */
		if (i < offsetof(dof_parsed_t, type) &&
		    i + ret >= offsetof(dof_parsed_t, type))
			sz = reply->size;

		/* Allocate more room if needed for the reply.  */
		if (sz > sizeof(dof_parsed_t)) {
			dof_parsed_t *new_reply;

			new_reply = realloc(reply, reply->size);
			if (!new_reply)
				goto err;

			memset(((char *) new_reply) + i + ret, 0, new_reply->size - (i + ret));
			reply = new_reply;
		}

		i += ret;
	}

	return reply;

err:
	free(reply);
	return NULL;
}

