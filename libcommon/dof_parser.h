/*
 * Oracle Linux DTrace; DOF parser interface with the outside world
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DOF_PARSER_H
#define	_DOF_PARSER_H

#include <inttypes.h>
#include <stddef.h>

#include <dtrace/dof.h>
#include <dtrace/helpers.h>

/*
 * Result of DOF probe parsing.  We receive a provider info structure, followed
 * by N probe info structures each of which is followed by possibly many
 * tracepoint info structures, all tagged.  Things not yet used for probe
 * creation (like args, translated types, etc) are not returned.
 *
 * On error, a DIT_ERR structure is returned with an error message.
 */

typedef enum dof_parsed_info {
	DIT_PROVIDER = 0,
	DIT_PROBE = 1,
	DIT_TRACEPOINT = 2,
	DIT_ERR = 3
} dof_parsed_info_t;

/*
 * Bump this whenever dof_parsed changes.
 *
 * Files consisting of arrays of dof_parsed have a single 64-bit word at the
 * start which is the version of the dof_parseds within it.  The data flowing
 * over the stream from the seccomped parser has no such prefix.
 */
#define DOF_PARSED_VERSION 1

typedef struct dof_parsed {
	/*
	 * Size of this instance of this structure.
	 */
	size_t size;

	dof_parsed_info_t type;

	__extension__ union {
		struct dpi_provider_info {
			/*
			 * Number of probes that follow.
			 */
			size_t nprobes;

			/*
			 * Provider name.
			 */
			char name[1];
		} provider;
		struct dpi_probe_info {
			/*
			 * Number of tracepoints that follow.
			 */
			size_t ntp;

			/*
			 * Probe module, function, and name (\0-separated).
			 */
			char name[1];
		} probe;

		struct dpi_tracepoint_info {
			/*
			 * Offset of this tracepoint.
			 */
			uint64_t addr;

			/*
			 * True if this is an is-enabled probe.
			 */
			uint32_t is_enabled;

			/*
			 * XXX Not yet implemented: name, args
			 */
		} tracepoint;

		struct dpi_err {
			/*
			 * An errno value.
			 */
			int err_no;

			/*
			 * A \0-terminated string.
			 */
			char err[1];
		} err;
	};
} dof_parsed_t;

/*
 * Host-side: in dof_parser_host.c.  The host is the
 * non-jailed process that talks to the jailed parser.
 */

/*
 * Write the DOF to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int dof_parser_host_write(int out, const dof_helper_t *dh, dof_hdr_t *dof);

/*
 * Read a single DOF structure from a parser pipe.  Wait at most TIMEOUT seconds
 * to do so.
 *
 * Returns NULL and sets errno on error.
 */
dof_parsed_t *dof_parser_host_read(int in, int timeout);

/* Parser-side: in dof_parser.c.  */

/*
 * Get a dof_helper_t from the input fd.
 *
 * Set OK to zero if no further parsing is possible.
 */
dof_helper_t *dof_copyin_helper(int in, int out, int *ok);

/*
 * Get a buffer of DOF from the input fd and sanity-check it.
 *
 * Set OK to zero if no further parsing is possible.
 */
dof_hdr_t *dof_copyin_dof(int in, int out, int *ok);

/*
 * Parse probe info out of the passed-in dof_helper_t and dof_hdr_t DOF buffer,
 * and pass it out of OUT in the form of a stream of dof_parser_info_t.
 */
void dof_parse(int out, dof_helper_t *dhp, dof_hdr_t *dof);

/*
 * Shared host and parser-side.
 */
/*
 * Write something to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int dof_parser_write_one(int out, const void *buf, size_t size);

#endif	/* _DOF_PARSER_H */
