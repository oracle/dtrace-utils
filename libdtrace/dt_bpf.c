/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

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

	dt_dprintf("Creating BPF map '%s' (ksz %u, vsz %u, sz %d)\n",
		   name, ksz, vsz, size);
	fd = bpf_create_map_name(type, name, ksz, vsz, size, 0);
	if (fd < 0) {
		dt_dprintf("failed to create BPF map '%s': %s\n",
			   name, strerror(errno));
		return -1;
	} else
		dt_dprintf("BPF map '%s' is FD %d (ksz %u, vsz %u, sz %d)\n",
			   name, fd, ksz, vsz, size);

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
 * - mem:	Output buffer scratch memory.  Thiss is implemented as a global
 *		per-CPU map with a singleton element (key 0).  This means that
 *		every CPU will see its own copy of this singleton element, and
 *		can use it without interference from other CPUs.  The size of
 *		the value (a byte array) is the maximum trace buffer record
 *		size that any of the compiled programs can emit.
 * - strtab:	String table map.  This is a global map with a singleton
 *		element (key 0) that contains the entire string table as a
 *		concatenation of all unique strings (each terminated with a
 *		NUL byte).  The string table size is taken from the DTrace
 *		consumer handle (dt_strlen).
 * - gvars:	Global variables map, associating a 64-bit value with each
 *		global variable.  The map is indexed by global variable id.
 *		The amount of global variables is the next-to--be-assigned
 *		global variable id minus the base id.
 *
 * FIXME: TLS variable storage is still being designed further so this is just
 *	  a temporary placeholder and will most likely be replaced by something
 *	  else.  If we stick to the legacy DTrace approach, we will need to
 *	  determine the maximum overall key size for TLS variables *and* the
 *	  maximum value size.  Based on these values, the legacy code would
 *	  take the memory size set aside for dynamic variables, and divide it by
 *	  the storage size needed for the largest dynamic variable (associative
 *	  array element or TLS variable).
 *
 * - tvars:	Thread-local (TLS) variables map, associating a 64-bit value
 *		with each thread-local variable.  The map is indexed by a value
 *		computed based on the thread-local variable id and execution
 *		thread information to ensure each thread has its own copy of a
 *		given thread-local variable.  The amount of TLS variable space
 *		to allocate for these dynamic variables is calculated based on
 *		the number of uniquely named TLS variables (next-to-be-assigned
 *		id minus the base id).
 * - probes:	Probe information map, associating a probe info structure with
 *		each probe that is used in the current probing session.
 */
int
dt_bpf_gmap_create(dtrace_hdl_t *dtp, uint_t probec)
{
	/* If we already created the global maps, return success. */
	if (dt_gmap_done)
		return 0;

	/* Mark global maps creation as completed. */
	dt_gmap_done = 1;

	return create_gmap(dtp, "buffers", BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			   sizeof(uint32_t), sizeof(uint32_t),
			   dtp->dt_conf.numcpus) &&
	       create_gmap(dtp, "mem", BPF_MAP_TYPE_PERCPU_ARRAY,
			   sizeof(uint32_t), dtp->dt_maxreclen, 1) &&
	       create_gmap(dtp, "strtab", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), dtp->dt_strlen, 1) &&
	       create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(uint64_t),
			   dt_idhash_peekid(dtp->dt_globals) -
				DIF_VAR_OTHER_UBASE) &&
	       create_gmap(dtp, "tvars", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(uint32_t),
			   dt_idhash_peekid(dtp->dt_tls) -
				DIF_VAR_OTHER_UBASE) &&
	       create_gmap(dtp, "probes", BPF_MAP_TYPE_ARRAY,
			   sizeof(uint32_t), sizeof(void *), probec);
	/* FIXME: Need to put in the actual struct ref for probe info. */
}
