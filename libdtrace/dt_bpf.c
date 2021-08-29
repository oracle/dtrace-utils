/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
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
#include <dt_dis.h>
#include <dt_dctx.h>
#include <dt_probe.h>
#include <dt_state.h>
#include <dt_string.h>
#include <dt_strtab.h>
#include <dt_bpf.h>
#include <dt_bpf_maps.h>
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

/*
 * Load the value for the given key in the map referenced by the given fd.
 */
int dt_bpf_map_lookup(int fd, const void *key, void *val)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;
	attr.value = (uint64_t)(unsigned long)val;

	return bpf(BPF_MAP_LOOKUP_ELEM, &attr);
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

static int
set_task_offsets(dtrace_hdl_t *dtp)
{
	ctf_id_t type;
	ctf_membinfo_t ctm;
	ctf_file_t *cfp = dtp->dt_shared_ctf;

	type = ctf_lookup_by_name(cfp, "struct task_struct");
	if (type == CTF_ERR)
		return -1;

	if (ctf_member_info(cfp, type, "real_parent", &ctm) == CTF_ERR)
		return -1;
	dt_state_set_offparent(dtp, ctm.ctm_offset / NBBY);

	if (ctf_member_info(cfp, type, "tgid", &ctm) == CTF_ERR)
		return -1;
	dt_state_set_offtgid(dtp, ctm.ctm_offset / NBBY);

	return 0;
}

static void
populate_probes_map(dtrace_hdl_t *dtp, int fd)
{
	dt_probe_t	*prp;

	for (prp = dt_list_next(&dtp->dt_enablings); prp != NULL;
	     prp = dt_list_next(prp)) {
		dt_bpf_probe_t	pinfo;

		pinfo.prv = dt_strtab_index(dtp->dt_ccstab, prp->desc->prv);
		pinfo.mod = dt_strtab_index(dtp->dt_ccstab, prp->desc->mod);
		pinfo.fun = dt_strtab_index(dtp->dt_ccstab, prp->desc->fun);
		pinfo.prb = dt_strtab_index(dtp->dt_ccstab, prp->desc->prb);

		dt_bpf_map_update(fd, &prp->desc->id, &pinfo);
	}
}

/*
 * Create the global BPF maps that are shared between all BPF programs in a
 * single tracing session:
 *
 * - state:	DTrace session state, used to communicate state between BPF
 *		programs and userspace.  The content of the map is defined in
 *		dt_state.h.
 * - aggs:	Aggregation data buffer map, associated with each CPU.  The
 *		map is implemented as a global per-CPU map with a singleton
 *		element (key 0).  Every aggregation is stored with two copies
 *		of its data to provide a lockless latch-based mechanism for
 *		atomic reading and writing.
 * - buffers:	Perf event output buffer map, associating a perf event output
 *		buffer with each CPU.  The map is indexed by CPU id.
 * - cpuinfo:	CPU information map, associating a cpuinfo_t structure with
 *		each online CPU on the system.
 * - mem:	Scratch memory.  This is implemented as a global per-CPU map
 *		with a singleton element (key 0).  This means that every CPU
 *		will see its own copy of this singleton element, and can use it
 *		without interference from other CPUs.  The scratch memory is
 *		used to store the DTrace machine state, the temporary output
 *		buffer, and temporary storage for stack traces, string
 *		manipulation, etc.
 * - strtab:	String table map.  This is a global map with a singleton
 *		element (key 0) that contains the entire string table as a
 *		concatenation of all unique strings (each terminated with a
 *		NUL byte).  The string table size is taken from the DTrace
 *		consumer handle (dt_strlen), and increased by the maximum
 *		string size to ensure that the BPF verifier can validate all
 *		access requests for dynamic references to string constants.
 * - probes:	Probe information map.  This is a global map indexed by probe
 *		ID.  The value is a struct that contains static probe info.
 *		The map only contains entries for probes that are actually in
 *		use.
 * - gvars:	Global variables map.  This is a global map with a singleton
 *		element (key 0) addressed by variable offset.
 * - lvars:	Local variables map.  This is a per-CPU map with a singleton
 *		element (key 0) addressed by variable offset.
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
	int		stabsz, gvarsz, lvarsz, tvarc, aggsz, memsz;
	int		ci_mapfd, st_mapfd, pr_mapfd;
	uint32_t	key = 0;

	/* If we already created the global maps, return success. */
	if (dt_gmap_done)
		return 0;

	/* Mark global maps creation as completed. */
	dt_gmap_done = 1;

	/* Determine the aggregation buffer size.  */
	aggsz = dt_idhash_datasize(dtp->dt_aggs);

	/* Determine sizes for global, local, and TLS maps. */
	gvarsz = P2ROUNDUP(dt_idhash_datasize(dtp->dt_globals), 8);
	lvarsz = P2ROUNDUP(dtp->dt_maxlvaralloc, 8);
	tvarc = dt_idhash_peekid(dtp->dt_tls) - DIF_VAR_OTHER_UBASE;

	/* Create global maps as long as there are no errors. */
	dtp->dt_stmap_fd = create_gmap(dtp, "state", BPF_MAP_TYPE_ARRAY,
				       sizeof(DT_STATE_KEY_TYPE),
				       sizeof(DT_STATE_VAL_TYPE),
				       DT_STATE_NUM_ELEMS);
	if (dtp->dt_stmap_fd == -1)
		return -1;		/* dt_errno is set for us */

	/*
	 * Check if there is aggregation data to be collected.
	 */
	if (aggsz > 0) {
		dtp->dt_aggmap_fd = create_gmap(dtp, "aggs",
						BPF_MAP_TYPE_PERCPU_ARRAY,
						sizeof(uint32_t), aggsz, 1);
		if (dtp->dt_aggmap_fd == -1)
			return -1;	/* dt_errno is set for us */
	}

	if (create_gmap(dtp, "buffers", BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			sizeof(uint32_t), sizeof(uint32_t),
			dtp->dt_conf.num_online_cpus) == -1)
		return -1;		/* dt_errno is set for us */

	ci_mapfd = create_gmap(dtp, "cpuinfo", BPF_MAP_TYPE_PERCPU_ARRAY,
			       sizeof(uint32_t), sizeof(cpuinfo_t), 1);
	if (ci_mapfd == -1)
		return -1;		/* dt_errno is set for us */

	/*
	 * The size of the map value (a byte array) is the sum of:
	 *	- size of the DTrace machine state, rounded up to the nearest
	 *	  multiple of 8
	 *	- 8 bytes padding for trace buffer alignment purposes
	 *	- maximum trace buffer record size, rounded up to the
	 *	  multiple of 8
	 *	- the greater of:
	 *		+ the maximum stack trace size
	 *		+ four times the maximum string size (incl. length)
	 *		  plus the maximum string size (to accomodate the BPF
	 *		  verifier)
	 */
	memsz = roundup(sizeof(dt_mstate_t), 8) +
		8 +
		roundup(dtp->dt_maxreclen, 8) +
		MAX(sizeof(uint64_t) * dtp->dt_options[DTRACEOPT_MAXFRAMES],
		    DT_TSTRING_SLOTS *
			(DT_STRLEN_BYTES + dtp->dt_options[DTRACEOPT_STRSIZE]) +
		    dtp->dt_options[DTRACEOPT_STRSIZE] + 1
		);
	if (create_gmap(dtp, "mem", BPF_MAP_TYPE_PERCPU_ARRAY,
			sizeof(uint32_t), memsz, 1) == -1)
		return -1;		/* dt_errno is set for us */

	/*
	 * We need to create the global (consolidated) string table.  We store
	 * the actual length (for in-code BPF validation purposes) but augment
	 * it by the maximum string size to determine the size of the BPF map
	 * value that is used to store the strtab.
	 */
	dtp->dt_strlen = dt_strtab_size(dtp->dt_ccstab);
	stabsz = dtp->dt_strlen +
		 ctf_type_size(DT_STR_CTFP(dtp), DT_STR_TYPE(dtp));
	dtp->dt_strtab = dt_zalloc(dtp, stabsz);
	if (dtp->dt_strtab == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	dt_strtab_write(dtp->dt_ccstab, (dt_strtab_write_f *)dt_strtab_copystr,
			dtp->dt_strtab);

	st_mapfd = create_gmap(dtp, "strtab", BPF_MAP_TYPE_ARRAY,
			       sizeof(uint32_t), stabsz, 1);
	if (st_mapfd == -1)
		return -1;		/* dt_errno is set for us */

	pr_mapfd = create_gmap(dtp, "probes", BPF_MAP_TYPE_HASH,
			       sizeof(uint32_t), sizeof(dt_bpf_probe_t),
			       dt_list_length(&dtp->dt_enablings));
	if (pr_mapfd == -1)
		return -1;		/* dt_errno is set for us */

	if (gvarsz > 0 &&
	    create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY,
			sizeof(uint32_t), gvarsz, 1) == -1)
		return -1;		/* dt_errno is set for us */

	if (lvarsz > 0 &&
	    create_gmap(dtp, "lvars", BPF_MAP_TYPE_PERCPU_ARRAY,
			sizeof(uint32_t), lvarsz, 1) == -1)
		return -1;		/* dt_errno is set for us */

	if (tvarc > 0 &&
	    create_gmap(dtp, "tvars", BPF_MAP_TYPE_ARRAY,
			sizeof(uint32_t), sizeof(uint64_t), tvarc) == -1)
		return -1;		/* dt_errno is set for us */

	/* Populate the 'cpuinfo' map. */
	dt_bpf_map_update(ci_mapfd, &key, dtp->dt_conf.cpus);

	/* Populate the 'strtab' map. */
	dt_bpf_map_update(st_mapfd, &key, dtp->dt_strtab);

	/* Populate the 'probes' map. */
	populate_probes_map(dtp, pr_mapfd);

	/* Set some task_struct offsets in state. */
	if (set_task_offsets(dtp))
		return dt_set_errno(dtp, EDT_CTF);

	return 0;
}

/*
 * Perform relocation processing for BPF maps in a program.
 */
static void
dt_bpf_reloc_prog(dtrace_hdl_t *dtp, const dtrace_difo_t *dp)
{
	int			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;
	struct bpf_insn		*text = dp->dtdo_buf;

	for (; len != 0; len--, rp++) {
		const char	*name = dt_difo_getstr(dp, rp->dofr_name);
		dt_ident_t	*idp = dt_idhash_lookup(dtp->dt_bpfsyms, name);
		int		ioff = rp->dofr_offset /
					sizeof(struct bpf_insn);
		uint32_t	val = 0;

		/*
		 * If the relocation is for a BPF map, fill in its fd.
		 */
		if (idp->di_kind == DT_IDENT_PTR) {
			if (rp->dofr_type == R_BPF_64_64)
				text[ioff].src_reg = BPF_PSEUDO_MAP_FD;

			val = idp->di_id;

			if (rp->dofr_type == R_BPF_64_64) {
				text[ioff].imm = val;
				text[ioff + 1].imm = 0;
			} else if (rp->dofr_type == R_BPF_64_32)
				text[ioff].imm = val;
		}
	}
}

/*
 * Load a BPF program into the kernel.
 *
 * Note that DTrace generates BPF programs that are licensed under the GPL.
 */
static int
dt_bpf_load_prog(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		 const dtrace_difo_t *dp, uint_t cflags)
{
	struct bpf_load_program_attr	attr;
	size_t				logsz;
	char				*log;
	int				rc, origerrno = 0;
	const dtrace_probedesc_t	*pdp = prp->desc;
	char				*p, *q;

	/*
	 * Check whether there are any BPF specific relocations that may need to
	 * be performed.  If so, we need to modify the executable code.  This
	 * can be done in-place since program loading is serialized.
	 */
	if (dp->dtdo_brelen)
		dt_bpf_reloc_prog(dtp, dp);

	memset(&attr, 0, sizeof(struct bpf_load_program_attr));

	DT_DISASM_PROG_FINAL(dtp, cflags, dp, stderr, NULL, prp->desc);

	attr.prog_type = prp->prov->impl->prog_type;
	attr.name = NULL;
	attr.insns = dp->dtdo_buf;
	attr.insns_cnt = dp->dtdo_len;
	attr.license = BPF_CG_LICENSE;

	if (dtp->dt_options[DTRACEOPT_BPFLOG] == DTRACEOPT_UNSET) {
		rc = bpf_load_program_xattr(&attr, NULL, 0);
		if (rc >= 0)
			return rc;

		origerrno = errno;
	}

	if (dtp->dt_options[DTRACEOPT_BPFLOGSIZE] != DTRACEOPT_UNSET)
		logsz = dtp->dt_options[DTRACEOPT_BPFLOGSIZE];
	else
		logsz = BPF_LOG_BUF_SIZE;
	attr.log_level = 4 | 2 | 1;
	log = dt_zalloc(dtp, logsz);
	assert(log != NULL);
	rc = bpf_load_program_xattr(&attr, log, logsz);
	if (rc < 0) {
		dt_bpf_error(dtp,
			     "BPF program load for '%s:%s:%s:%s' failed: %s\n",
			     pdp->prv, pdp->mod, pdp->fun, pdp->prb,
			     strerror(origerrno ? origerrno : errno));

		/* check whether we have an incomplete BPF log */
		if (errno == ENOSPC) {
			fprintf(stderr,
				"BPF verifier log is incomplete and is not reported.\n"
				"Set DTrace option 'bpflogsize' to some greater size for more output.\n"
				"(Current size is %ld.)\n", logsz);
			goto out;
		}
	} else if (dtp->dt_options[DTRACEOPT_BPFLOG] == DTRACEOPT_UNSET)
		goto out;

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

out:
	dt_free(dtp, log);

	return rc >= 0 ? rc : -1;
}

/*
 * Perform relocation processing on the ERROR probe program.
 */
static void
dt_bpf_reloc_error_prog(dtrace_hdl_t *dtp, dtrace_difo_t *dp)
{
	int			len = dp->dtdo_brelen;
	const dof_relodesc_t	*rp = dp->dtdo_breltab;
	dof_relodesc_t		*nrp = dp->dtdo_breltab;
	struct bpf_insn		*text = dp->dtdo_buf;

	for (; len != 0; len--, rp++) {
		const char	*name = dt_difo_getstr(dp, rp->dofr_name);
		dt_ident_t	*idp = dt_idhash_lookup(dtp->dt_bpfsyms, name);
		int		ioff = rp->dofr_offset /
					sizeof(struct bpf_insn);

		/*
		 * We need to turn "call dt_error" into a NOP.  We also remove
		 * the relocation record because it is obsolete.
		 */
		if (idp != NULL && idp->di_kind == DT_IDENT_SYMBOL &&
		    strcmp(idp->di_name, "dt_error") == 0) {
			text[ioff] = BPF_NOP();
			continue;
		}

		if (nrp != rp)
			*nrp = *rp;

		nrp++;
	}

	dp->dtdo_brelen -= rp - nrp;
}

int
dt_bpf_load_progs(dtrace_hdl_t *dtp, uint_t cflags)
{
	dt_probe_t	*prp;
	dtrace_difo_t	*dp;
	dt_ident_t	*idp = dt_dlib_get_func(dtp, "dt_error");
	dtrace_optval_t	dest_ok = DTRACEOPT_UNSET;

	assert(idp != NULL);

	/*
	 * First construct the ERROR probe program (to be included in probe
	 * programs that may trigger a fault).
	 *
	 * After constructing the program, we need to patch up any calls to
	 * dt_error because DTrace cannot handle faults in ERROR itself.
	 */
	dp = dt_program_construct(dtp, dtp->dt_error, cflags, idp);
	if (dp == NULL)
		return -1;

	idp->di_flags |= DT_IDFLG_CGREG;	/* mark it as inline-ready */
	dt_bpf_reloc_error_prog(dtp, dp);

	/*
	 * Determine whether we can allow destructive actions.
	 */
	dtrace_getopt(dtp, "destructive", &dest_ok);

	/*
	 * Now construct all the other programs.
	 */
	for (prp = dt_list_next(&dtp->dt_enablings); prp != NULL;
	     prp = dt_list_next(prp)) {
		int		fd, rc;

		/* Already done. */
		if (prp == dtp->dt_error)
			continue;

		dp = dt_program_construct(dtp, prp, cflags, NULL);
		if (dp == NULL)
			return -1;
		if (dp->dtdo_flags & DIFOFLG_DESTRUCTIVE &&
		    dest_ok == DTRACEOPT_UNSET)
			return dt_set_errno(dtp, EDT_DESTRUCTIVE);

		fd = dt_bpf_load_prog(dtp, prp, dp, cflags);
		if (fd < 0)
			return fd;

		dt_difo_free(dtp, dp);

		if (!prp->prov->impl->attach)
			return -1;
		rc = prp->prov->impl->attach(dtp, prp, fd);
		if (rc < 0)
			return rc;
	}

	return 0;
}
