/*
 * Oracle Linux DTrace; USDT definitions parser interface.
 * Copyright (c) 2022, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_USDT_PARSER_H
#define	_USDT_PARSER_H

#include <inttypes.h>
#include <stddef.h>
#include <libelf.h>

#include <dtrace/dof.h>
#include <dtrace/helpers.h>

/*
 * Data transfer unit for the DOF parser.
 */
typedef struct usdt_data	usdt_data_t;
struct usdt_data {
	size_t		size;
	void		*buf;
	usdt_data_t	*next;
};

/*
 * Result of USDT definitions parsing.  The order of elements in the parsed
 * stream is:
 *
 * DIT_PROVIDER (at least 1, which contains...)
 *   DIT_PROBE (at least 1, each of which has...)
 *     DIT_ARGS_NATIVE (1, optional)
 *     DIT_ARGS_XLAT (1, optional)
 *     DIT_ARGS_MAP (1, optional)
 *     DIT_TRACEPOINT (any number >= 1)
 * DIT_EOF (no more providers, last record)
 *
 * The dof_parsed.provider.flags word indicates the presence of the
 * various optional args records in the following stream (you can rely on
 * them if it simplifies things, but you don't have to).
 *
 * On error, a DIT_ERR structure is returned with an error message.
 */

typedef enum dof_parsed_info {
	DIT_PROVIDER = 0,
	DIT_PROBE = 1,
	DIT_TRACEPOINT = 2,
	DIT_ERR = 3,
	DIT_EOF = 4,
	DIT_ARGS_NATIVE = 5,
	DIT_ARGS_XLAT = 6,
	DIT_ARGS_MAP = 7,
} dof_parsed_info_t;

/*
 * Bump this whenever dof_parsed changes.
 *
 * Files consisting of arrays of dof_parsed have a single 64-bit word at the
 * start which is the version of the dof_parseds within it.  The data flowing
 * over the stream from the seccomped parser has no such prefix.
 */
#define DOF_PARSED_VERSION 2

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
			 * Number of native arguments that follow (if > 0, a
			 * DIT_ARGS_NATIVE will be received).
			 */
			size_t nargc;

			/*
			 * Number of xlated arguments that follow (if > 0, a
			 * DIT_ARGS_XLAT and DIT_ARGS_MAP will be received).
			 */
			size_t xargc;

			/*
			 * Probe module, function, and name (\0-separated).
			 */
			char name[1];
		} probe;

		/* V2+ only.  */
		struct dpi_probe_args_native_info {
			/*
			 * Array of native args.  nargc in length.
			 */
			char args[1];
		} nargs;

		/* V2+ only.  */
		struct dpi_probe_args_xlat_info {
			/*
			 * Array of translated args.  xargc in length.
			 */
			char args[1];
		} xargs;

		/*
		 * V2+ only.
		 */
		struct dpi_probe_args_map_info {
			/*
			 * Mapping from native arg index to xlated arg index.
			 * xargc in length.
			 */
			int8_t argmap[1];
		} argmap;

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
 * Host-side: in usdt_parser_host.c.
 * The host is the non-jailed process that talks to the jailed parser.
 */

/*
 * Write the USDT definitions data to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int usdt_parser_host_write(int out, const dof_helper_t *dh,
			   const usdt_data_t *data);

/*
 * Read a single dof_parsed_t structure from a parser pipe.  Wait at most
 * TIMEOUT seconds to do so.
 *
 * Returns NULL and sets errno on error.
 */
dof_parsed_t *usdt_parser_host_read(int in, int timeout);

/* Parser-side: in usdt_parser.c.  */

/*
 * Report a parser error.
 */
void usdt_error(int out, int err_no, const char *fmt, ...);

/*
 * Get a dof_helper_t from the input fd.
 *
 * Returns NULL on failure - no further processing is possible in that case.
 */
dof_helper_t *usdt_copyin_helper(int in);

/*
 * Get a USDT data block from the input fd.
 *
 * Set OK to zero if no further parsing is possible.
 */
usdt_data_t *usdt_copyin_data(int in, int out, int *ok);

/*
 * Parse probe info out of the passed-in dof_helper_t and USDT definitions data
 * block and emit it to OUT in the form of a stream of dof_parser_info_t.
 */
void usdt_parse(int out, dof_helper_t *dhp, usdt_data_t *data);

/*
 * Parse probe info out of the passed-in dof_helper_t and DOF section data and
 * emit it to OUT in the form of a stream of dof_parser_info_t.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int usdt_parse_dof(int out, dof_helper_t *dhp, dof_hdr_t *dof);

/*
 * Shared host and parser-side.
 */

/*
 * Write something to the parser pipe OUT.
 *
 * Returns 0 on success or a positive errno value on error.
 */
int usdt_parser_write_one(int out, const void *buf, size_t size);

#endif	/* _USDT_PARSER_H */
