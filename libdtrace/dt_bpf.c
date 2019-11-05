/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#if 0
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <port.h>
#include <dt_parser.h>
#include <dt_program.h>
#include <dt_grammar.h>
#endif

#include <errno.h>
#include <string.h>
#include <dtrace.h>
#include <dt_impl.h>

#include <bpf.h>

static bool dt_gmap_done = 0;

static int
create_gmap(dtrace_hdl_t *dtp, const char *name, enum bpf_map_type type,
	    int ksz, int vsz, int size)
{
	int		fd;
	dt_ident_t	*idp;

	fd = bpf_create_map_name(type, name, ksz, vsz, size, 0);
	if (fd < 0) {
		dt_dprintf("failed to create BPF map '%s': %s\n",
			   name, strerror(errno));
		return -1;
	} else
		dt_dprintf("BPF map '%s' is FD %d\n", name, fd);

	idp = dt_dlib_get_map(dtp, name);
	if (idp == NULL) {
		dt_dprintf("cannot find BPF map '%s'\n", name);
		close(fd);
		return -1;
	}

	((dtrace_syminfo_t *)idp->di_data)->id = fd;

	return fd;
}

/*
 * Create the global BPF maps that are shared between all BPF programs in a
 * single tracing session:
 *
 * - buffers:	Perf event output buffer map, associating a perf event output
 *		buffer with each CPU.  The map is indexed by CPU id.
 * - gvars:	Global variables map, associating a 64-bit value with each
 *		global variable.  The map is indexed by global variable id.
 * - tvars:	Thread-local variables map, associating a 64-bit value with
 *		each thread-local variable.  The map is indexed by a value
 *		computed based on the thread-local variable id and execution
 *		thread information to ensure each thread has its own copy of a
 *		given thread-local variable.
 * - probes:	Probe information map, associating a probe info structure with
 *		each probe that is used in the current probing session.
 */
int
dt_bpf_gmap_create(dtrace_hdl_t *dtp, uint_t gvarc, uint_t tvarc, uint_t probec)
{
	/* If we already created the global maps, return success. */
	if (dt_gmap_done)
		return 0;

	/* Mark global maps creation as completed. */
	dt_gmap_done = 1;

	return create_gmap(dtp, "buffers", BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			   sizeof(uint32_t), sizeof(uint32_t),
			   dtp->dt_conf.numcpus) &&
	       create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(uint64_t), gvarc) &&
	       create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(uint32_t), tvarc) &&
	       create_gmap(dtp, "probes", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(void *), probec);
	/* FIXME: Need to put in the actual struct ref for probe info. */
}
