/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <dtrace.h>
#include <dt_impl.h>
#include <dt_probe.h>
#include <port.h>

#include <bpf.h>

static bool dt_gmap_done = 0;

#define BPF_CG_LICENSE	"GPL";

static inline int perf_event_open(struct perf_event_attr *attr, pid_t pid,
				  int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static inline int bpf(enum bpf_cmd cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(union bpf_attr));
}

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

	/*
	 * Assign the fd as id for the BPF map identifier.
	 */
	idp = dt_dlib_get_map(dtp, name);
	if (idp == NULL) {
		dt_dprintf("cannot find BPF map '%s'\n", name);
		close(fd);
		return -1;
	}

	dt_ident_set_id(idp, fd);

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

/*
 * Perform relocation processing on a program.
 */
static void
dt_bpf_reloc_prog(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		  const dtrace_difo_t *dp)
{
	int			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;

	for (; len != 0; len--, rp++) {
		char		*name = &dp->dtdo_strtab[rp->dofr_name];
		dt_ident_t	*idp = dt_idhash_lookup(dtp->dt_bpfsyms, name);
		struct bpf_insn	*text = dp->dtdo_buf;
		int		ioff = rp->dofr_offset /
					sizeof(struct bpf_insn);

		if (rp->dofr_type == R_BPF_64_64) {
			text[ioff].src_reg = BPF_PSEUDO_MAP_FD;
			text[ioff].imm = idp->di_id;
			text[ioff + 1].imm = 0;
		}
	}
}

/*
 * Load a BPF program into the kernel.
 *
 * Note that DTrace generates BPF programs that are licensed under the GPL.
 */
int
dt_bpf_load_prog(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		 const dtrace_difo_t *dp)
{
	struct bpf_load_program_attr	attr;
	int				logsz = BPF_LOG_BUF_SIZE;
	char				*log;
	int				rc;

	/*
	 * Check whether there are any probe-specific relocations to be
	 * performed.  If so, we need to modify the executable code.  This can
	 * be done in-place since program loading is serialized.
	 *
	 * Relocations that are probe independent were already done at an
	 * earlier time so we can ignore those.
	 */
	if (dp->dtdo_brelen)
		dt_bpf_reloc_prog(dtp, prp, dp);

	memset(&attr, 0, sizeof(struct bpf_load_program_attr));

	log = dt_zalloc(dtp, logsz);
	assert(log != NULL);

	attr.prog_type = prp->prov->impl->prog_type;
	attr.name = NULL;
	attr.insns = dp->dtdo_buf;
	attr.insns_cnt = dp->dtdo_len;
	attr.license = BPF_CG_LICENSE;
	attr.log_level = 4 | 2 | 1;

	rc = bpf_load_program_xattr(&attr, log, logsz);
	if (rc < 0) {
		const dtrace_probedesc_t	*pdp = prp->desc;
		char				*p, *q;

		rc = -errno;
		fprintf(stderr,
			"BPF program load for '%s:%s:%s:%s' failed: %s\n",
			pdp->prv, pdp->mod, pdp->fun, pdp->prb, strerror(-rc));

		/*
		 * If there is BPF verifier output, print it with a "BPF: "
		 * prefix so it is easier to distinguish.
		 */
		for (p = log; p && *p; p = q) {
			q = strchr(p, '\n');

			if (q)
				*q++ = '\0';

			fprintf(stderr, "BPF: %s\n", p);
		}
	}

	dt_free(dtp, log);

	return rc;
}

/*
 * Attach a BPF program to a probe.
 */
int
dt_bpf_attach(dtrace_hdl_t *dtp, dt_probe_t *prp, int bpf_fd)
{
	/*
	 * If we have not yet created a perf event for this probe, do that now
	 * and cache the file descriptor so we only need to do this once.
	 */
	if (prp->event_fd == -1) {
		int			fd;
		struct perf_event_attr	attr = { 0, };

		attr.type = PERF_TYPE_TRACEPOINT;
		attr.sample_type = PERF_SAMPLE_RAW;
		attr.sample_period = 1;
		attr.wakeup_events = 1;
		attr.config = prp->event_id;

		fd = perf_event_open(&attr, -1, 0, -1, 0);
		if (fd < 0)
			return -errno;

		prp->event_fd = fd;
	}

	if (ioctl(prp->event_fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0)
		return -errno;

	return 0;
}

int
dt_bpf_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *sdp,
	    void *data)
{
	dtrace_probedesc_t	*pdp = &sdp->dtsd_ecbdesc->dted_probe;
	dtrace_actdesc_t	*ap = sdp->dtsd_action;
	dtrace_probeinfo_t	pip;
	dt_probe_t		*prp;
	int			fd, rc;
	int			*cnt = data;

	memset(&pip, 0, sizeof(pip));
	prp = dt_probe_info(dtp, pdp, &pip);
	if (!prp)
		return -ESRCH;

	/*
	 * In the new implementation, every statement has a single action,
	 * which is the entire program.  At a future code reworking, we should
	 * combine the action into the statement and do away with the action
	 * list.
	 */
	assert(ap == sdp->dtsd_action_last);
	fd = dt_bpf_load_prog(dtp, prp, ap->dtad_difo);
	if (fd < 0)
		return fd;

	rc = dt_bpf_attach(dtp, prp, fd);
	if (rc < 0)
		return rc;

	(*cnt)++;

	return 0;
}

int
dt_bpf_prog(dtrace_hdl_t *dtp, dtrace_prog_t *pgp)
{
	int	cnt = 0;
	int	rc;

	rc = dtrace_stmt_iter(dtp, pgp, dt_bpf_stmt, &cnt);

	return (rc < 0) ? rc : cnt;
}
