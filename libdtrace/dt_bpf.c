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
#include <dt_bpf.h>
#include <port.h>

#include <bpf.h>

static bool dt_gmap_done = 0;

#define BPF_CG_LICENSE	"GPL";

int
perf_event_open(struct perf_event_attr *attr, pid_t pid,
		    int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int
bpf(enum bpf_cmd cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(union bpf_attr));
}

static int
dt_bpf_error(dtrace_hdl_t *dtp, const char *fmt, ...)
{
	va_list	ap, apc;

	va_start(ap, fmt);
	va_copy(apc, ap);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, fmt, ap);
	va_end(ap);
	dt_debug_printf("bpf", fmt, apc);
	va_end(apc);

	return dt_set_errno(dtp, EDT_BPF);
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
	if (fd < 0)
		return dt_bpf_error(dtp, "failed to create BPF map '%s': %s\n",
				    name, strerror(errno));

	dt_dprintf("BPF map '%s' is FD %d (ksz %u, vsz %u, sz %d)\n",
		   name, fd, ksz, vsz, size);

	/*
	 * Assign the fd as id for the BPF map identifier.
	 */
	idp = dt_dlib_get_map(dtp, name);
	if (idp == NULL) {
		close(fd);
		return dt_bpf_error(dtp, "cannot find BPF map '%s'\n", name);
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
 * - cpuinfo:	CPU information map, associating a cpuinfo_t structure with
 *		each online CPU on the system.
 * - mem:	Scratch memory.  This is implemented as a global per-CPU map
 *		with a singleton element (key 0).  This means that every CPU
 *		will see its own copy of this singleton element, and can use it
 *		without interference from other CPUs.  The scratch memory is
 *		used to store the DTrace context and the temporary output
 *		buffer.  The size of the map value (a byte array) is the size
 *		of the DTrace context, rounded up to a multiple of 8 bytes,
 *		plus 8 bytes padding, plus the maximum trace buffer record size
 *		that any of the compiled programs can emit, rounded up to a
 *		multiple of 8 bytes.
 *		The 8 bytes padding is used to ensure proper trace data
 *		alignment.
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
 */
int
dt_bpf_gmap_create(dtrace_hdl_t *dtp)
{
	int		gvarc, tvarc;
	int		ci_mapfd;
	uint32_t	key = 0;

	/* If we already created the global maps, return success. */
	if (dt_gmap_done)
		return 0;

	/* Mark global maps creation as completed. */
	dt_gmap_done = 1;

	/* Determine the number of global and TLS variables. */
	gvarc = dt_idhash_peekid(dtp->dt_globals) - DIF_VAR_OTHER_UBASE;
	tvarc = dt_idhash_peekid(dtp->dt_tls) - DIF_VAR_OTHER_UBASE;

	/* Create global maps as long as there are no errors. */
	if (create_gmap(dtp, "buffers", BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			sizeof(uint32_t), sizeof(uint32_t),
			dtp->dt_conf.num_online_cpus) == -1)
		return -1;	/* dt_errno is set for us */

	ci_mapfd = create_gmap(dtp, "cpuinfo", BPF_MAP_TYPE_PERCPU_ARRAY,
			       sizeof(uint32_t), sizeof(cpuinfo_t), 1);
	if (ci_mapfd == -1)
		return -1;	/* dt_errno is set for us */

	if (create_gmap(dtp, "mem", BPF_MAP_TYPE_PERCPU_ARRAY,
			sizeof(uint32_t),
			roundup(sizeof(dt_mstate_t), 8) + 8 +
				roundup(dtp->dt_maxreclen, 8), 1) == -1)
		return -1;	/* dt_errno is set for us */

	if (create_gmap(dtp, "strtab", BPF_MAP_TYPE_ARRAY,
			sizeof(uint32_t), dtp->dt_strlen, 1) == -1)
		return -1;	/* dt_errno is set for us */

	if (gvarc > 0 &&
	    create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY,
			sizeof(uint32_t), sizeof(uint64_t), gvarc) == -1)
		return -1;	/* dt_errno is set for us */

	if (tvarc > 0 &&
	    create_gmap(dtp, "tvars", BPF_MAP_TYPE_ARRAY,
			sizeof(uint32_t), sizeof(uint64_t), tvarc) == -1)
		return -1;	/* dt_errno is set for us */

	/* Populate the 'cpuinfo' map. */
	dt_bpf_map_update(ci_mapfd, &key, dtp->dt_conf.cpus);

	return 0;
}

/*
 * Store the (key, value) pair in the map referenced by the given fd.
 */
int dt_bpf_map_update(int fd, const void *key, const void *val)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;
	attr.value = (uint64_t)(unsigned long)val;
	attr.flags = 0;

	return bpf(BPF_MAP_UPDATE_ELEM, &attr);
}

/*
 * Perform relocation processing on a program.
 */
static void
dt_bpf_reloc_prog(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		  const dtrace_difo_t *dp, dtrace_epid_t epid)
{
	int			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;

	for (; len != 0; len--, rp++) {
		char		*name = &dp->dtdo_strtab[rp->dofr_name];
		dt_ident_t	*idp = dt_idhash_lookup(dtp->dt_bpfsyms, name);
		struct bpf_insn	*text = dp->dtdo_buf;
		int		ioff = rp->dofr_offset /
					sizeof(struct bpf_insn);
		uint32_t	val = 0;

		switch (idp->di_kind) {
		case DT_IDENT_FUNC:	/* built-in function */
			continue;
		case DT_IDENT_PTR:	/* BPF map */
			if (rp->dofr_type == R_BPF_64_64)
				text[ioff].src_reg = BPF_PSEUDO_MAP_FD;

			val = idp->di_id;
			break;
		case DT_IDENT_SCALAR:	/* built-in constant */
			switch (idp->di_id) {
			case DT_CONST_EPID:
				val = epid;
				break;
			case DT_CONST_ARGC:
				val = 0;	/* FIXME */
				break;
			}

			break;
		case DT_IDENT_SYMBOL:	/* BPF function (pre-compiled) */
			continue;
		}

		if (rp->dofr_type == R_BPF_64_64) {
			text[ioff].imm = val;
			text[ioff + 1].imm = 0;
		} else if (rp->dofr_type == R_BPF_64_32)
			text[ioff].imm = val;
	}
}

/*
 * Load a BPF program into the kernel.
 *
 * Note that DTrace generates BPF programs that are licensed under the GPL.
 */
int
dt_bpf_load_prog(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		 dtrace_stmtdesc_t *sdp)
{
	struct bpf_load_program_attr	attr;
	dtrace_epid_t			epid;
	const dtrace_difo_t		*dp;
	int				logsz = BPF_LOG_BUF_SIZE;
	char				*log;
	int				rc;

	dp = dt_dlib_get_func_difo(dtp, sdp->dtsd_clause);
	epid = dt_epid_add(dtp, dp->dtdo_ddesc, prp->desc->id);

	/*
	 * Check whether there are any probe-specific relocations to be
	 * performed.  If so, we need to modify the executable code.  This can
	 * be done in-place since program loading is serialized.
	 *
	 * Relocations that are probe independent were already done at an
	 * earlier time so we can ignore those.
	 */
	if (dp->dtdo_brelen)
		dt_bpf_reloc_prog(dtp, prp, dp, epid);

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

		rc = dt_bpf_error(dtp,
				  "BPF program load for '%s:%s:%s:%s' failed: "
				  "%s\n",
				  pdp->prv, pdp->mod, pdp->fun, pdp->prb,
				  strerror(errno));

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
	tp_probe_t	*datap = prp->prv_data;

	/*
	 * If we have not yet created a perf event for this probe, do that now
	 * and cache the file descriptor so we only need to do this once.
	 */
	if (datap->event_fd == -1) {
		int			fd;
		struct perf_event_attr	attr = { 0, };

		attr.type = PERF_TYPE_TRACEPOINT;
		attr.sample_type = PERF_SAMPLE_RAW;
		attr.sample_period = 1;
		attr.wakeup_events = 1;
		attr.config = datap->event_id;

		fd = perf_event_open(&attr, -1, 0, -1, 0);
		if (fd < 0)
			return dt_set_errno(dtp, errno);

		datap->event_fd = fd;
	}

	if (ioctl(datap->event_fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0)
		return dt_set_errno(dtp, errno);

	return 0;
}

int
dt_bpf_stmt(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *sdp,
	    void *data)
{
	dtrace_probedesc_t	*pdp = &sdp->dtsd_ecbdesc->dted_probe;
	dtrace_probeinfo_t	pip;
	dt_probe_t		*prp;
	int			fd, rc;
	int			*cnt = data;

	memset(&pip, 0, sizeof(pip));
	prp = dt_probe_info(dtp, pdp, &pip);
	if (!prp)
		return dt_set_errno(dtp, ESRCH);

	fd = dt_bpf_load_prog(dtp, prp, sdp);
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
