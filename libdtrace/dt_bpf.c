/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates. All rights reserved.
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
#include <linux/btf.h>
#include <dt_btf.h>
#include <port.h>

static boolean_t	dt_gmap_done = 0;

#define BPF_CG_LICENSE	"GPL";

int
dt_perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
		   int group_fd, unsigned long flags)
{
	int	rc;

	rc = syscall(__NR_perf_event_open, attr, pid, cpu, group_fd,
		     flags | PERF_FLAG_FD_CLOEXEC);
	return rc >= 0 ? rc : -errno;
}

int
dt_bpf(enum bpf_cmd cmd, union bpf_attr *attr)
{
	int	rc;

	rc = syscall(__NR_bpf, cmd, attr, sizeof(union bpf_attr));
	return rc >= 0 ? rc : -errno;
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

int
dt_bpf_lockmem_error(dtrace_hdl_t *dtp, const char *msg)
{
	return dt_bpf_error(dtp, "%s:\n"
			    "\tThe kernel locked-memory limit is possibly too low.  Set a\n"
			    "\thigher limit with the DTrace option '-xlockmem=N'.  Or, use\n"
			    "\t'ulimit -l N' (Kbytes).  Or, make N the string 'unlimited'.\n"
			    , msg);
}

/*
 * Load a BPF program into the kernel (and attach it to an object by BTF id if
 * specified).
 */
int
dt_bpf_prog_attach(enum bpf_prog_type ptype, enum bpf_attach_type atype,
		   int btf_fd, uint32_t btf_id, const dtrace_difo_t *dp,
		   uint32_t log_level, char *log_buf, size_t log_buf_sz)
{
	union bpf_attr	attr;
	int		fd;
	int		i = 0;

	memset(&attr, 0, sizeof(attr));
	attr.prog_type = ptype;
	attr.expected_attach_type = atype;
	attr.attach_btf_obj_fd = btf_fd;
	attr.attach_btf_id = btf_id;
	attr.insn_cnt = dp->dtdo_len;
	attr.insns = (uint64_t)dp->dtdo_buf;
	attr.license = (uint64_t)BPF_CG_LICENSE;
	attr.log_level = log_level;

	if (log_level > 0) {
		attr.log_buf = (uint64_t)log_buf;
		attr.log_size = log_buf_sz;
	}

	/* Syscall could return EAGAIN - try at most 5 times. */
	do {
		fd = dt_bpf(BPF_PROG_LOAD, &attr);
	} while (fd == -EAGAIN && ++i < 5);

	return fd;
}

/*
 * Load the BPF program for a probe into the kernel.
 */
int
dt_bpf_prog_load(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		 const dtrace_difo_t *dp, uint32_t lvl, char *buf, size_t sz)
{
	return dt_bpf_prog_attach(prp->prov->impl->prog_type, 0, 0, 0, dp,
				  lvl, buf, sz);
}

/*
 * Get BTF dict information based on its fd.
 */
int
dt_bpf_btf_get_info_by_fd(int fd, btf_info_t *info, uint32_t *size)
{
	union bpf_attr	attr;
	int		rc;

	memset(&attr, 0, sizeof(attr));
	attr.info.bpf_fd = fd;
	attr.info.info = (uint64_t)info;
	attr.info.info_len = *size;

	rc = dt_bpf(BPF_OBJ_GET_INFO_BY_FD, &attr);
	if (rc == 0)
		*size = attr.info.info_len;

	return rc;
}

/*
 * Get a file descriptor for a BTF dict based on its id.
 */
int
dt_bpf_btf_get_fd_by_id(uint32_t id)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.btf_id = id;

	return dt_bpf(BPF_BTF_GET_FD_BY_ID, &attr);
}

/*
 * Get the id for the "next" BTF dict based on a given id (0 to get the first).
 */
int
dt_bpf_btf_get_next_id(uint32_t curr, uint32_t *next)
{
	union bpf_attr	attr;
	int		rc;

	memset(&attr, 0, sizeof(attr));
	attr.start_id = curr;

	rc = dt_bpf(BPF_BTF_GET_NEXT_ID, &attr);
	if (rc == 0)
		*next = attr.next_id;

	return rc;
}

/*
 * Create a named BPF map.
 */
static int
dt_bpf_map_create(enum bpf_map_type map_type, const char *name,
		  uint32_t key_size, uint32_t value_size, uint32_t max_entries,
		  uint32_t map_flags)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_type = map_type;
	attr.key_size = key_size;
	attr.value_size = value_size;
	attr.max_entries = max_entries;
	attr.map_flags = map_flags;
	if (name)
		memcpy(attr.map_name, name, MIN(strlen(name),
						sizeof(attr.map_name) - 1));

	return dt_bpf(BPF_MAP_CREATE, &attr);
}

/*
 * Create a named BPF meta map (array of maps or hash of maps).
 */
static int
dt_bpf_map_create_meta(enum bpf_map_type otype, const char *name,
		       uint32_t oksz, uint32_t osize, uint32_t oflags,
		       enum bpf_map_type itype,
		       uint32_t iksz, uint32_t ivsz, uint32_t isize,
		       uint32_t iflags)
{
	union bpf_attr	attr;
	int		ifd, ofd;

	/* Create (temporary) inner map as template. */
	memset(&attr, 0, sizeof(attr));
	attr.map_type = itype;
	attr.key_size = iksz;
	attr.value_size = ivsz;
	attr.max_entries = isize;
	attr.map_flags = iflags;

	ifd =  dt_bpf(BPF_MAP_CREATE, &attr);
	if (ifd < 0)
		return ifd;

	/* Create the map-of-maps. */
	memset(&attr, 0, sizeof(attr));
	attr.map_type = otype;
	attr.key_size = oksz;
	attr.value_size = sizeof(uint32_t);
	attr.inner_map_fd = ifd;
	attr.max_entries = osize;
	attr.map_flags = oflags;
	if (name)
		memcpy(attr.map_name, name, MIN(strlen(name),
						sizeof(attr.map_name) - 1));

	ofd = dt_bpf(BPF_MAP_CREATE, &attr);

	/* Done with the template map. */
	close(ifd);

	return ofd;
}

/*
 * Get a file descriptor for a BPF map based on its id.
 */
int
dt_bpf_map_get_fd_by_id(uint32_t id)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_id = id;

	return dt_bpf(BPF_MAP_GET_FD_BY_ID, &attr);
}

/*
 * Load the value for the given key in the map referenced by the given fd.
 */
int
dt_bpf_map_lookup(int fd, const void *key, void *val)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;
	attr.value = (uint64_t)(unsigned long)val;

	return dt_bpf(BPF_MAP_LOOKUP_ELEM, &attr);
}

/*
 * Find the next key after the given key in the map referenced by the given fd.
 */
int
dt_bpf_map_next_key(int fd, const void *key, void *nxt)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;
	attr.next_key = (uint64_t)(unsigned long)nxt;

	return dt_bpf(BPF_MAP_GET_NEXT_KEY, &attr);
}

/*
 * Delete the given key from the map referenced by the given fd.
 */
int
dt_bpf_map_delete(int fd, const void *key)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;

	return dt_bpf(BPF_MAP_DELETE_ELEM, &attr);
}

/*
 * Store the (key, value) pair in the map referenced by the given fd.
 */
int
dt_bpf_map_update(int fd, const void *key, const void *val)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.map_fd = fd;
	attr.key = (uint64_t)(unsigned long)key;
	attr.value = (uint64_t)(unsigned long)val;
	attr.flags = BPF_ANY;

	return dt_bpf(BPF_MAP_UPDATE_ELEM, &attr);
}

/*
 * Retrieve the fd for a map-in-map, i.e. map[okey] which is the fd of a map.
 *
 * Note: the caller is responsible for closing the fd.
 */
int
dt_bpf_map_lookup_fd(int fd, const void *okey)
{
	uint32_t	id;

	if (dt_bpf_map_lookup(fd, okey, &id) < 0)
		return -1;

	return dt_bpf_map_get_fd_by_id(id);
}

/*
 * Retrieve the value in a map-of-maps, i.e. map[okey][ikey].
 */
int
dt_bpf_map_lookup_inner(int fd, const void *okey, const void *ikey, void *val)
{
	int		rc;

	fd = dt_bpf_map_lookup_fd(fd, okey);
	if (fd < 0)
		return -1;

	rc = dt_bpf_map_lookup(fd, ikey, val);
	close(fd);

	return rc;
}

/*
 * Store the value in a map-of-maps, i.e. map[okey][ikey] = value.
 */
int
dt_bpf_map_update_inner(int fd, const void *okey, const void *ikey,
			const void *val)
{
	uint32_t	id;
	int		rc;

	if (dt_bpf_map_lookup(fd, okey, &id) < 0)
		return -1;

	fd = dt_bpf_map_get_fd_by_id(id);
	if (fd < 0)
		return -1;

	rc = dt_bpf_map_update(fd, ikey, val);
	close(fd);

	return rc;
}

/*
 * Associate a BPF program (by fd) with a raw tracepoint.
 */
int
dt_bpf_raw_tracepoint_open(const void *tp, int fd)
{
	union bpf_attr	attr;

	memset(&attr, 0, sizeof(attr));
	attr.raw_tracepoint.name = (uint64_t)(unsigned long)tp;
	attr.raw_tracepoint.prog_fd = fd;

	return dt_bpf(BPF_RAW_TRACEPOINT_OPEN, &attr);
}

static int
have_helper(uint32_t func_id)
{
	char		log[DT_BPF_LOG_SIZE_SMALL] = { 0, };
	char		*ptr = &log[0];
	struct bpf_insn	insns[] = {
				BPF_CALL_HELPER(func_id),
				BPF_RETURN()
			};
	dtrace_difo_t	dp;
	int		fd;

	dp.dtdo_buf = insns;
	dp.dtdo_len = ARRAY_SIZE(insns);

	/* If the program loads, we can use the helper. */
	fd = dt_bpf_prog_attach(BPF_PROG_TYPE_KPROBE, 0, 0, 0, &dp,
			      1, log, DT_BPF_LOG_SIZE_SMALL);
	if (fd > 0) {
		close(fd);
		return 1;
	}

	/* Permission denied -> helper not available to us */
	if (errno == EPERM)
		return 0;

	/* Sentinel to make the loop and string searches safe. */
	log[DT_BPF_LOG_SIZE_SMALL - 2] = ' ';
	log[DT_BPF_LOG_SIZE_SMALL - 1] = 0;

	while (*ptr == 0)
		ptr++;

	/*
	 * If an 'invalid func' or 'unknown func' failure is reported, we
	 * cannot use the helper.
	 */
	return strstr(ptr, "invalid func") == NULL &&
	       strstr(ptr, "unknown func") == NULL;
}

static void
dt_bpf_init_helpers(dtrace_hdl_t *dtp)
{
	uint32_t	i;

	/* Assume all helpers are available. */
	for (i = 0; i < __BPF_FUNC_MAX_ID; i++)
		dtp->dt_bpfhelper[i] = i;

	/* Verify the existence of some specific helpers. */
#define BPF_HELPER_MAP(orig, alt)	\
	if (!have_helper(BPF_FUNC_##orig)) \
			dtp->dt_bpfhelper[BPF_FUNC_##orig] = BPF_FUNC_##alt;

	BPF_HELPER_MAP(probe_read_user, probe_read);
	BPF_HELPER_MAP(probe_read_user_str, probe_read_str);
	BPF_HELPER_MAP(probe_read_kernel, probe_read);
	BPF_HELPER_MAP(probe_read_kernel_str, probe_read_str);
	BPF_HELPER_MAP(get_current_task_btf, unspec);
	BPF_HELPER_MAP(task_pt_regs, unspec);
#undef BPF_HELPER_MAP
}

static int
have_attach_type(enum bpf_prog_type ptype, enum bpf_attach_type atype,
		 uint32_t btf_id)
{
	struct bpf_insn	insns[] = {
				BPF_MOV_IMM(BPF_REG_0, 0),
				BPF_RETURN()
			};
	dtrace_difo_t	dp;
	int		fd;

	dp.dtdo_buf = insns;
	dp.dtdo_len = ARRAY_SIZE(insns);

	fd = dt_bpf_prog_attach(ptype, atype, 0, btf_id, &dp, 0, NULL, 0);
	/* If the program loads, we can use the attach type. */
	if (fd > 0) {
		close(fd);
		return 1;
	}

	/* Failed -> attach type not available to us */
	return 0;
}

static void
dt_bpf_init_features(dtrace_hdl_t *dtp)
{
	uint32_t	btf_id;

	btf_id = dt_btf_lookup_name_kind(dtp, dtp->dt_shared_btf, "bpf_check",
					 BTF_KIND_FUNC);
	if (btf_id >= 0 &&
	    have_attach_type(BPF_PROG_TYPE_TRACING, BPF_TRACE_FENTRY, btf_id))
		BPF_SET_FEATURE(dtp, BPF_FEAT_FENTRY);
}

void
dt_bpf_init(dtrace_hdl_t *dtp)
{
	dt_bpf_init_helpers(dtp);
	dt_bpf_init_features(dtp);
}

static int
map_create_error(dtrace_hdl_t *dtp, const char *name, int err)
{
	char	msg[64];

	snprintf(msg, sizeof(msg),
		 "failed to create BPF map '%s'", name);

	if (err == E2BIG)
		return dt_bpf_error(dtp, "%s: Too big\n", msg);
	if (err == EPERM)
		return dt_bpf_lockmem_error(dtp, msg);

	return dt_bpf_error(dtp, "%s: %s\n", msg, strerror(err));
}

static int
create_gmap(dtrace_hdl_t *dtp, const char *name, enum bpf_map_type type,
	    size_t ksz, size_t vsz, size_t size)
{
	int		fd, err;
	dt_ident_t	*idp;

	dt_dprintf("Creating BPF map '%s' (ksz %lu, vsz %lu, sz %lu)\n",
		   name, ksz, vsz, size);

	if (ksz > UINT_MAX || vsz > UINT_MAX || size > UINT_MAX) {
		fd = -1;
		err = E2BIG;
	} else {
		fd = dt_bpf_map_create(type, name, ksz, vsz, size, 0);
		err = errno;
	}

	if (fd < 0)
		return map_create_error(dtp, name, err);

	dt_dprintf("BPF map '%s' is FD %d\n", name, fd);

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
create_gmap_of_maps(dtrace_hdl_t *dtp, const char *name,
		    enum bpf_map_type otype, size_t oksz, size_t osize,
		    enum bpf_map_type itype, size_t iksz, size_t ivsz,
		    size_t isize)
{
	int		fd, err;
	dt_ident_t	*idp;

	dt_dprintf("Creating BPF meta map '%s' (ksz %lu, sz %lu)\n",
		   name, oksz, osize);
	dt_dprintf("    storing BPF maps (ksz %lu, vsz %lu, sz %lu)\n",
		   iksz, ivsz, isize);

	if (oksz > UINT_MAX || osize > UINT_MAX ||
	    iksz > UINT_MAX || ivsz > UINT_MAX || isize > UINT_MAX) {
		fd = -1;
		err = E2BIG;
	} else {
		fd = dt_bpf_map_create_meta(otype, name, oksz, osize, 0,
					    itype, iksz, ivsz, isize, 0);
		err = errno;
	}

	if (fd < 0)
		return map_create_error(dtp, name, err);

	dt_dprintf("BPF map '%s' is FD %d\n", name, fd);

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
 * Create the 'state' BPF map.
 *
 * DTrace session state, used to communicate state between BPF programs and
 * userspace.  The content of the map is defined in dt_state.h.
 */
static int
gmap_create_state(dtrace_hdl_t *dtp)
{
	dtp->dt_stmap_fd = create_gmap(dtp, "state", BPF_MAP_TYPE_ARRAY,
				       sizeof(DT_STATE_KEY_TYPE),
				       sizeof(DT_STATE_VAL_TYPE),
				       DT_STATE_NUM_ELEMS);

	return dtp->dt_stmap_fd;
}

/*
 * Create the 'aggs' BPF map (and its companion 'agggen' BPF map).
 *
 * - aggs:	Aggregation data buffer map, associated with each CPU.  The map
 *		is implemented as a global array-of-maps indexed by CPU id.
 *		The associated value is a map with a singleton element (key 0).
 * - agggen:	Aggregation generation counters.  The map associates a
 *		generation counter with each aggregation ID.  The counter is
 *		used to determine whether the aggregation data is valid.  The
 *		initial value is 1.
 */
static int
gmap_create_aggs(dtrace_hdl_t *dtp)
{
	size_t		ncpus = dtp->dt_conf.max_cpuid + 1;
	size_t		nelems = 0;
	uint32_t	aggc =  dt_idhash_peekid(dtp->dt_aggs);
	uint32_t	i;

	/* Only create the map if it is used. */
	if (dtp->dt_maxaggdsize == 0)
		return 0;

	assert(dtp->dt_maxtuplesize > 0);

	nelems = dtp->dt_options[DTRACEOPT_AGGSIZE] /
		 (dtp->dt_maxtuplesize + dtp->dt_maxaggdsize);
	if (nelems == 0)
		return dt_set_errno(dtp, EDT_BUFTOOSMALL);

	dtp->dt_aggmap_fd = create_gmap_of_maps(dtp, "aggs",
						BPF_MAP_TYPE_ARRAY_OF_MAPS,
						sizeof(uint32_t), ncpus,
						BPF_MAP_TYPE_HASH,
						dtp->dt_maxtuplesize,
						dtp->dt_maxaggdsize, nelems);
	if (dtp->dt_aggmap_fd == -1)
		return -1;

	for (i = 0; i < dtp->dt_conf.num_online_cpus; i++) {
		int	cpu = dtp->dt_conf.cpus[i].cpu_id;
		char	name[16];
		int	fd;

		snprintf(name, 16, "aggs_%d", cpu);
		fd = dt_bpf_map_create(BPF_MAP_TYPE_HASH, name,
				       dtp->dt_maxtuplesize,
				       dtp->dt_maxaggdsize, nelems, 0);
		if (fd < 0)
			return map_create_error(dtp, name, errno);

		dt_bpf_map_update(dtp->dt_aggmap_fd, &cpu, &fd);
	}

	/* Create the agg generation value array. */
	dtp->dt_genmap_fd = create_gmap(dtp, "agggen", BPF_MAP_TYPE_ARRAY,
					sizeof(uint32_t), sizeof(uint64_t),
					aggc);
	if (dtp->dt_genmap_fd == -1)
		 return -1;

	for (i = 0; i < aggc; i++) {
		uint64_t	val = 1;

		if (dt_bpf_map_update(dtp->dt_genmap_fd, &i, &val) == -1)
			return dt_bpf_error(dtp,
					    "cannot update BPF map 'agggen': %s\n",
					    strerror(errno));
	}

	return 0;
}

/*
 * Create the 'specs' BPF map.
 *
 * Map associating speculation IDs with a dt_bpf_specs_t struct giving the
 * number of buffers speculated into for this speculation, and the number
 * drained by userspace.
 */
static int
gmap_create_specs(dtrace_hdl_t *dtp)
{
	return create_gmap(dtp, "specs", BPF_MAP_TYPE_HASH, sizeof(uint32_t),
			   sizeof(dt_bpf_specs_t),
			   dtp->dt_options[DTRACEOPT_NSPEC]);
}

/*
 * Create the 'buffers' BPF map.
 *
 * Perf event output buffer map, associating a perf event output buffer with
 * each CPU.  The map is indexed by CPU id.
 */
static int
gmap_create_buffers(dtrace_hdl_t *dtp)
{
	return create_gmap(dtp, "buffers", BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			   sizeof(uint32_t), sizeof(uint32_t),
			   dtp->dt_conf.num_online_cpus);
}

/*
 * Create the 'cpuinfo' BPF map.
 *
 * CPU information map, associating a cpuinfo_t structure with each online CPU
 * on the system.
 */
static int
gmap_create_cpuinfo(dtrace_hdl_t *dtp)
{
	int			i, rc;
	uint32_t		key = 0;
	dtrace_conf_t		*conf = &dtp->dt_conf;
	size_t			ncpus = conf->num_online_cpus;
	dt_bpf_cpuinfo_t	*data;
	cpuinfo_t		*ci;

	/*
	 * num_possible_cpus <= num_online_cpus: see dt_conf_init.
	 */
	data = dt_calloc(dtp, dtp->dt_conf.num_possible_cpus,
			 sizeof(dt_bpf_cpuinfo_t));
	if (data == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	for (i = 0, ci = &conf->cpus[0]; i < ncpus; i++, ci++)
		memcpy(&data[ci->cpu_id].ci, ci, sizeof(cpuinfo_t));

	dtp->dt_cpumap_fd = create_gmap(dtp, "cpuinfo",
					BPF_MAP_TYPE_PERCPU_ARRAY,
					sizeof(uint32_t),
					sizeof(dt_bpf_cpuinfo_t), 1);
	if (dtp->dt_cpumap_fd == -1)
		return -1;

	rc = dt_bpf_map_update(dtp->dt_cpumap_fd, &key, data);
	dt_free(dtp, data);
	if (rc == -1)
		return dt_bpf_error(dtp,
				    "cannot update BPF map 'cpuinfo': %s\n",
				    strerror(errno));

	return 0;
}

/*
 * Create the 'mem' BPF map.
 *
 * CPU local storage.  This is implemented as a global per-CPU map with a
 * singleton element (key 0).  This means that every CPU will see its own copy
 * of this singleton element, and can use it without interference from other
 * CPUs.  The local storage is used to store the DTrace machine state, the
 * temporary output buffer, and temporary storage for stack traces, string
 * manipulation, etc.
 *
 * The size of the memory region is the sum of:
 *	- size of the DTrace machine state, rounded up to the nearest
 *	  multiple of 8
 *	- 8 bytes padding for trace buffer alignment purposes
 *	- maximum trace buffer record size, rounded up to the nearest
 *	  multiple of 8
 *	- size of dctx->mem (see dt_dctx.h)
 */
static int
gmap_create_mem(dtrace_hdl_t *dtp)
{
	size_t	sz = roundup(sizeof(dt_mstate_t), 8) +
		     8 +
		     roundup(dtp->dt_maxreclen, 8) +
		     DMEM_SIZE(dtp);

	return create_gmap(dtp, "mem", BPF_MAP_TYPE_PERCPU_ARRAY,
			   sizeof(uint32_t), sz, 1);
}

/*
 * Create the 'scratchmem' BPF map.
 *
 * Storage for alloca() and other per-clause scratch space, implemented just as
 * for mem.
 *
 * The size for this is twice what it needs to be, to allow us to bcopy things
 * the size of the scratch space to the start of the scratch space without
 * tripping verifier failures: see dt_cg_check_bounds.
 */
static int
gmap_create_scratchmem(dtrace_hdl_t *dtp)
{
	size_t		sz = dtp->dt_options[DTRACEOPT_SCRATCHSIZE];

	/* Only create the map if it is used. */
	if (sz == 0)
		return 0;

	return create_gmap(dtp, "scratchmem", BPF_MAP_TYPE_PERCPU_ARRAY,
			   sizeof(uint32_t), sz * 2, 1);
}

/*
 * Create the 'strtab' BPF map.
 *
 * String table map.  This is a global map with a singleton element (key 0)
 * that contains the entire string table as a concatenation of all unique
 * strings (each terminated with a NUL byte).  The string table size is taken
 * from the DTrace consumer handle (dt_strlen).
 *
 * The read-only data is appended to the string table, as is a memory block of
 * zeros for initializing memory regions.
 *
 * In order to ensure the BPF verifier can validate all access requests for
 * dynamic references to string constants, the size of the read-only data plus
 * the size of the block of zeros must at least match the maximum string size.
 * The size of the block of zeros must at least match the maximum read-only
 * item size, and be large enough to satisfy all 0-initialization needs in all
 * BPF programs being loaded for this tracing session.
 */
static int
gmap_create_strtab(dtrace_hdl_t *dtp)
{
	size_t		sz;
	uint8_t		*buf, *end;
	char		*strtab;
	size_t		strsize = dtp->dt_options[DTRACEOPT_STRSIZE];
	uint32_t	key = 0;
	int		fd, rc, err;

	dtp->dt_strlen = dt_strtab_size(dtp->dt_ccstab);
	dtp->dt_rooffset = P2ROUNDUP(dtp->dt_strlen, 8);
	dtp->dt_rosize = dt_rodata_size(dtp->dt_rodata);
	dtp->dt_zerooffset = P2ROUNDUP(dtp->dt_rooffset + dtp->dt_rosize, 8);
	dtp->dt_zerosize = 0;
	sz = dt_rodata_max_item_size(dtp->dt_rodata);

	/*
	 * Ensure the zero-filled memory at the end of the strtab is large
	 * enough to accommodate all needs for such a memory block.
	 */
	if (dtp->dt_rosize < strsize + 1)
		dtp->dt_zerosize = strsize + 1 - dtp->dt_rosize;
	if (dtp->dt_zerosize < sz)
		dtp->dt_zerosize = sz;
	if (dtp->dt_zerosize < dtp->dt_maxdvarsize)
		dtp->dt_zerosize = dtp->dt_maxdvarsize;
	if (dtp->dt_zerosize < dtp->dt_maxtuplesize)
		dtp->dt_zerosize = dtp->dt_maxtuplesize;
	if (dtp->dt_zerosize < dtp->dt_maxaggdsize)
		dtp->dt_zerosize = dtp->dt_maxaggdsize;

	sz = dtp->dt_zerooffset + dtp->dt_zerosize;
	strtab = dt_zalloc(dtp, sz);
	if (strtab == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	/* Copy the string table data. */
	dt_strtab_write(dtp->dt_ccstab, (dt_strtab_write_f *)dt_strtab_copystr,
			strtab);

	/* Loop over the string table and truncate strings that are too long. */
	buf = (uint8_t *)strtab;
	end = buf + dtp->dt_strlen;
	while (buf < end) {
		uint_t	len = strlen((char *)buf);

		if (len > strsize)
			buf[strsize] = '\0';

		buf += len + 1;
	}

	/* Copy the read-only data. */
	dt_rodata_write(dtp->dt_rodata, (dt_rodata_copy_f *)dt_rodata_copy,
			strtab + dtp->dt_rooffset);

	fd = create_gmap(dtp, "strtab", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t),
			 sz, 1);
	if (fd == -1)
		return -1;

	rc = dt_bpf_map_update(fd, &key, strtab);
	err = errno;
	dt_free(dtp, strtab);

	if (rc == -1)
		return dt_bpf_error(dtp, "cannot update BPF map 'strtab': %s\n",
				    strerror(err));

	return 0;
}

/*
 * Create the 'probes' BPF map.
 *
 * Probe information map.  This is a global map indexed by probe ID.  The value
 * is a struct that contains static probe info.  The map only contains entries
 * for probes that are actually in use.
 */
static int
gmap_create_probes(dtrace_hdl_t *dtp)
{
	int		fd;
	dt_probe_t	*prp;

	fd = create_gmap(dtp, "probes", BPF_MAP_TYPE_HASH, sizeof(uint32_t),
			 sizeof(dt_bpf_probe_t),
			 dt_list_length(&dtp->dt_enablings));
	if (fd == -1)
		return -1;

	for (prp = dt_list_next(&dtp->dt_enablings); prp != NULL;
	     prp = dt_list_next(prp)) {
		dt_bpf_probe_t	pinfo;

		pinfo.prv = dt_strtab_index(dtp->dt_ccstab, prp->desc->prv);
		pinfo.mod = dt_strtab_index(dtp->dt_ccstab, prp->desc->mod);
		pinfo.fun = dt_strtab_index(dtp->dt_ccstab, prp->desc->fun);
		pinfo.prb = dt_strtab_index(dtp->dt_ccstab, prp->desc->prb);

		if (dt_bpf_map_update(fd, &prp->desc->id, &pinfo) == -1)
			return dt_bpf_error(
				dtp, "cannot update BPF map 'probes': %s\n",
				strerror(errno));
	}

	return 0;
}

/*
 * Create the 'gvars' BPF map.
 *
 * Global variables map.  This is a global map with a singleton element (key 0)
 * addressed by variable offset.
 */
static int
gmap_create_gvars(dtrace_hdl_t *dtp)
{
	size_t	sz = dt_idhash_datasize(dtp->dt_globals);

	/* Only create the map if it is used. */
	if (sz == 0)
		return 0;

	return create_gmap(dtp, "gvars", BPF_MAP_TYPE_ARRAY, sizeof(uint32_t),
			   sz, 1);
}

/*
 * Create the 'lvars' BPF map.
 *
 * Local variables map.  This is a per-CPU map with a singleton element (key 0)
 * addressed by variable offset.
 */
static int
gmap_create_lvars(dtrace_hdl_t *dtp)
{
	size_t	sz = P2ROUNDUP(dtp->dt_maxlvaralloc, 8);

	/* Only create the map if it is used. */
	if (sz == 0)
		return 0;

	return create_gmap(dtp, "lvars", BPF_MAP_TYPE_PERCPU_ARRAY,
			   sizeof(uint32_t), sz, 1);
}

/*
 * Create the 'dvars' BPF map (and its companion 'tuples' BPF map).
 *
 * - dvars:	Dynamic variables map.  This is a global hash map indexed with
 *		a unique numeric identifier for each dynamic variable (thread
 *		local variable or associative array element).  The value size
 *		is the largest dynamic variable size across all programs in the
 *		tracing session.
 * - tuples:	Tuple-to-id map.  This is a global hash map indexed with a
 *		tuple.  The value associated with the tuple key is an id that
 *		is used to index the dvars map.  The key size is determined as
 *		the largest tuple used across all programs in the tracing
 *		session.
 */
static int
gmap_create_dvars(dtrace_hdl_t *dtp)
{
	size_t	nelems = 0;

	/* Only create the map if it is used. */
	if (dtp->dt_maxdvarsize == 0)
		return 0;

	nelems = dtp->dt_options[DTRACEOPT_DYNVARSIZE] /
		 (dtp->dt_maxtuplesize + dtp->dt_maxdvarsize);
	if (nelems == 0)
		return dt_set_errno(dtp, EDT_BUFTOOSMALL);

	if (create_gmap(dtp, "dvars", BPF_MAP_TYPE_HASH, sizeof(uint64_t),
			dtp->dt_maxdvarsize, nelems) == -1)
		return -1;

	/* Only create the map if it is used. */
	if (dtp->dt_maxtuplesize == 0)
		return 0;

	return create_gmap(dtp, "tuples", BPF_MAP_TYPE_HASH,
			   dtp->dt_maxtuplesize, sizeof(uint64_t), nelems);
}

/*
 * Create the global BPF maps that are shared between all BPF programs in a
 * single tracing session.
 */
int
dt_bpf_gmap_create(dtrace_hdl_t *dtp)
{
	/* If we already created the global maps, return success. */
	if (dt_gmap_done)
		return 0;

	/* Mark global maps creation as completed. */
	dt_gmap_done = 1;

#define CREATE_MAP(name) \
	if (gmap_create_##name(dtp) == -1) \
		return -1;

	CREATE_MAP(state)
	CREATE_MAP(aggs)
	CREATE_MAP(specs)
	CREATE_MAP(buffers)
	CREATE_MAP(cpuinfo)
	CREATE_MAP(mem)
	CREATE_MAP(scratchmem)
	CREATE_MAP(strtab)
	CREATE_MAP(probes)
	CREATE_MAP(gvars)
	CREATE_MAP(lvars)
	CREATE_MAP(dvars)
#undef CREATE_MAP

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
	size_t				logsz;
	char				*log;
	int				rc, origerr = 0;
	const dtrace_probedesc_t	*pdp = prp->desc;
	char				*p, *q;

	/*
	 * Check whether there are any BPF specific relocations that may need to
	 * be performed.  If so, we need to modify the executable code.  This
	 * can be done in-place since program loading is serialized.
	 */
	if (dp->dtdo_brelen)
		dt_bpf_reloc_prog(dtp, dp);

	DT_DISASM_PROG_FINAL(dtp, cflags, dp, stderr, NULL, prp->desc);

	if (dtp->dt_options[DTRACEOPT_BPFLOG] == DTRACEOPT_UNSET) {
		rc = prp->prov->impl->load_prog(dtp, prp, dp, 0, NULL, 0);
		if (rc >= 0)
			return rc;

		origerr = -rc;
	}

	if (dtp->dt_options[DTRACEOPT_BPFLOGSIZE] != DTRACEOPT_UNSET)
		logsz = dtp->dt_options[DTRACEOPT_BPFLOGSIZE];
	else
		logsz = DT_BPF_LOG_SIZE_DEFAULT;
	log = dt_zalloc(dtp, logsz);
	assert(log != NULL);
	rc = prp->prov->impl->load_prog(dtp, prp, dp, 4 | 2 | 1, log, logsz);
	if (rc < 0) {
		char	msg[64];
		int	err = -rc;

		snprintf(msg, sizeof(msg),
			 "BPF program load for '%s:%s:%s:%s' failed",
			 pdp->prv, pdp->mod, pdp->fun, pdp->prb);
		if (err == EPERM)
			return dt_bpf_lockmem_error(dtp, msg);
		dt_bpf_error(dtp, "%s: %s\n", msg,
		     strerror(origerr ? origerr : err));

		/* check whether we have an incomplete BPF log */
		if (err == ENOSPC) {
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
dt_bpf_make_progs(dtrace_hdl_t *dtp, uint_t cflags)
{
	dt_probe_t	*prp;
	dtrace_difo_t	*dp;
	dt_ident_t	*idp = dt_dlib_get_func(dtp, "dt_error");

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
	 * Now construct all the other programs.
	 */
	for (prp = dt_list_next(&dtp->dt_enablings); prp != NULL;
	     prp = dt_list_next(prp)) {
		/* Already done. */
		if (prp == dtp->dt_error)
			continue;

		/*
		 * Enabled probes with no trampoline act like they exist but
		 * no code is generated for them.
		 */
		if (prp->prov->impl->prog_type == BPF_PROG_TYPE_UNSPEC)
			continue;

		dp = dt_construct(dtp, prp, cflags, NULL);
		if (dp == NULL)
			return -1;

		DT_DISASM_PROG(dtp, cflags, dp, stderr, NULL, prp->desc);

		prp->difo = dp;
	}

	return 0;
}

int
dt_bpf_load_progs(dtrace_hdl_t *dtp, uint_t cflags)
{
	dt_probe_t	*prp;
	dtrace_optval_t	dest_ok = DTRACEOPT_UNSET;

	/*
	 * Determine whether we can allow destructive actions.
	 */
	dtrace_getopt(dtp, "destructive", &dest_ok);

	for (prp = dt_list_next(&dtp->dt_enablings); prp != NULL;
	     prp = dt_list_next(prp)) {
		dtrace_difo_t	*dp = prp->difo;
		int		fd;
		int		rc = -1;

		if (dp == NULL)
			continue;

		if (dt_link(dtp, prp, dp, NULL) == -1)
			return -1;

		DT_DISASM_PROG_LINKED(dtp, cflags, dp, stderr, NULL, prp->desc);

		if (dp->dtdo_flags & DIFOFLG_DESTRUCTIVE &&
		    dest_ok == DTRACEOPT_UNSET)
			return dt_set_errno(dtp, EDT_DESTRUCTIVE);

		fd = dt_bpf_load_prog(dtp, prp, dp, cflags);
		if (fd == -1)
			return -1;

		if (prp->prov->impl->attach)
			rc = prp->prov->impl->attach(dtp, prp, fd);

		if (rc == -ENOTSUPP) {
			char	*s;

			close(fd);
			if (asprintf(&s, "Failed to enable %s:%s:%s:%s",
				     prp->desc->prv, prp->desc->mod,
				     prp->desc->fun, prp->desc->prb) == -1)
				return dt_set_errno(dtp, EDT_ENABLING_ERR);
			dt_handle_rawerr(dtp, s);
			free(s);
		} else if (rc < 0) {
			close(fd);
			return dt_set_errno(dtp, EDT_ENABLING_ERR);
		}
	}

	return 0;
}
