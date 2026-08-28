/*
 * Oracle Linux DTrace; DOF state storage management.
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The DOF stash is the principal state preservation mechanism used by dtprobed.
 * Keeping DOF on disk keeps memory management complexity low, makes it
 * possible for dtprobed to track DOF across daemon restarts and upgrades,
 * and gives the rest of dtrace a place to acquire the DOF contributed by
 * processes on the system for USDT probe operations.
 *
 * The state storage structure is as follows:
 *
 * /run/dtrace/stash/: Things private to dtprobed.
 *
 *    .../dof/$dev-$ino: USDT definition data contributed by particular
 *    mappings, in raw form (as received from some probe-containing program).
 *
 *    .../dof/$dev-$ino-$n: Additional USDT definition data contributed by
 *    particular mappings, in raw form (as received from some probe-containing
 *    program).
 *
 *    .../dof-pid/$pid/$dev-$ino/: contains everything relating to DOF
 *    contributed by a particular USDT-containing ELF object within a given
 *    process (henceforth "DOF source").  Each DTRACEHIOC_ADDDOF ioctl call
 *    creates one of these.
 *
 *    .../dof-pid/$pid/$gen: symlink to a single $dev-$ino dir. "$gen" is an
 *    incrementing generation counter starting at zero for every unique PID,
 *    used to uniquely identify DOF pieces to remove.
 *
 *    .../dof-pid/$pid/next-gen: The next generation counter to use for a new
 *    piece of DOF from this process.  Encoded as a sparse file whose size is
 *    the next counter value to use.
 *
 *    .../dof-pid/$pid/exec-mapping: a dev/ino pair in the form
 *    $dev-$ino (as in the $dev-$ino directory entries): the dev/ino of the
 *    process's primary text mapping, as given by libproc.
 *
 *    .../dof-pid/$pid/$dev-$ino/raw and
 *    .../dof-pid/$pid/$dev-$ino/raw-$n: hardlinks to the USDT definition data
 *    for a given probe-containing program.  Pruned of dead processes at
 *    startup and on occasion: entries also deleted on receipt of
 *    DTRACEHIOC_REMOVE ioctls.  A hardlink is used in order to bump the link
 *    count for the corresponding data in the dof/ directory: when this link
 *    count falls to 1, the data is considered dead and the corresponding probe
 *    is removed.
 *
 *    .../dof-pid/$pid/$dev-$ino/dh: Raw form of the dof_helper_t received from
 *    a given DTRACEHIOC_ADDDOF, serialized straight to disk with no changes.
 *    Preserved for all DTRACEHIOC_ADDDOFs, even if the DOF was already
 *    contributed by some other process.  Keeping this around allows the DOF to
 *    be re-parsed as needed, even by a later re-execution of a different
 *    version of the same daemon with a different dof_parsed_t structure, and to
 *    be re-parsed with appropriate addresses for whichever mapping of this DOF
 *    is under consideration.
 *
 *    .../dof-pid/$pid/$dev-$ino/parsed/: Directory containing parsed DOF for a
 *    given DOF source.  Deleted when empty.
 *
 *    .../dof-pid/$pid/$dev-$ino/parsed/$prv:$mod:$fun:$prb: parsed DOF for a
 *    given PID/mapping/probe triplet.  Generated from the helper for PIDs that
 *    are the first appearance of a given piece of DOF.  Deleted at daemon
 *    startup if the dof_parsed_t structure changed.  The file consists of a
 *    uint64_t version number (DOF_PARSED_VERSION), then a stream of
 *    dof_parsed_t records, the first of type DIT_PROVIDER, the second
 *    DIT_PROBE, then optionally some DIT_*ARGS records (if the count of native
 *    args in the probe is >0, you get a DIT_NATIVE_ARGS: if the count of xlated
 *    args is >0, you get DIT_XLAT_ARGS and DIT_MAP_ARGS), then as many struct
 *    dof_parsed_t's of type DIT_TRACEPOINT as the DIT_PROBE record specifies.
 *
 * /run/dtrace/probes/: Per-probe info, written by dtprobed, read by DTrace.
 *
 *    .../$pid/$prv$pid/$mod/$fun/$prb: Hardlink from $prv:$mod:$fun:$prb
 *    above; parsed representation of one probe in a given process. Removed by
 *    dtprobed when the process dies, or if all mappings containing the probe
 *    are unmmapped.  Used by DTrace for tracing by PID.
 *
 *    (If a probe exists in multiple different mappings in a given process, they
 *    must all be identical.)
 *
 * In future, symlinks in /run/dtrace/probes/$prv$pid may exist, used for
 * globbing provider names across processes.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "dof_stash.h"

#include <dt_list.h>

#ifdef HAVE_FUSE_LOG
#include <fuse_log.h>
#else
#include "rpl_fuse_log.h"
#endif

#include <port.h>

static int dof_dir, pid_dir, probes_dir;

/*
 * Like close(), but only try to close positively-numbered fds.
 */
static int
maybe_close(int fd)
{
	if (fd >= 0)
		return close(fd);
	return 0;
}

/*
 * Make a runtime state directory and return its fd.
 */
static int
make_state_dir(const char *dir)
{
	int fd;

	if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: "
			 "cannot create state directory %s: %s\n",
			 dir, strerror(errno));
		return -1;
	}

	if ((fd = open(dir, O_PATH | O_CLOEXEC)) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: "
			 "cannot open state directory %s: %s\n",
			 dir, strerror(errno));
		return -1;
	}
	return fd;
}

/*
 * Make a state directory under some other directory and return an fd to it.
 *
 * If "readable" is set, the fd is opened with O_RDONLY rather than O_PATH.
 */
static int
make_state_dirat(int dirfd, const char *name, const char *errmsg, int readable)
{
	int fd;
	int flags = O_PATH;

	if (readable)
		flags = O_RDONLY;

	if (mkdirat(dirfd, name, 0755) < 0) {
		if (errno == EEXIST) {
			struct stat s;

			if (fstatat(dirfd, name, &s, 0) == 0 &&
			    S_ISDIR(s.st_mode))
				goto open_fd;
			errno = EEXIST;
		}
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot create %s dir named %s: %s\n",
			 errmsg, name, strerror(errno));
		return -1;
	}

open_fd:
	if ((fd = openat(dirfd, name, flags | O_CLOEXEC)) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot open %s dir named %s: %s\n",
			 errmsg, name, strerror(errno));
		return -1;
	}
	return fd;
}

/*
 * Initialize the DOF-stashing machinery, if needed.
 */
int
dof_stash_init(const char *statedir)
{
	int root;
	int stash;

	if (statedir == NULL)
		statedir="/run/dtrace";

	if ((root = make_state_dir(statedir)) < 0)
		return -1;

	if ((stash = make_state_dirat(root, "stash", "statedir", 0)) < 0)
		return -1;

	if ((dof_dir = make_state_dirat(stash, "dof", "DOF", 0)) < 0)
		return -1;
	if ((pid_dir = make_state_dirat(stash, "dof-pid", "per-pid DOF", 1)) < 0)
		return -1;

	if ((probes_dir = make_state_dirat(root, "probes", "probesdir", 1)) < 0)
		return -1;

	close(root);
	close(stash);

	return 0;
}

/*
 * Compose the provider+pid name of the per-PID instance of a provider from
 * pieces.
 */
static char *
make_provpid_name(const char *prov, pid_t pid)
{
	char *ret;

	if (asprintf(&ret, "%s%li", prov, (long) pid) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory making provider name\n");
		return NULL;
	}
	return ret;
}

/*
 * Ban "." and ".." as probe description components. Components are also not
 * allowed to contain any '/' character.
 */
int
is_component_unsafe(const char *s)
{
	return strcmp(s, ".") == 0 || strcmp(s, "..") == 0 ||
	       strchr(s, '/') != NULL;
}

/*
 * Compose a full probespec's name from pieces.
 */
static char *
make_probespec_name(const char *prov, const char *mod, const char *fn,
		    const char *prb)
{
	char *ret;

	/*
	 * Since probe description components are used in the creation of paths
	 * that will be written to, make sure their content is safe.
	 */
	if (is_component_unsafe(prov) || is_component_unsafe(mod) ||
	    is_component_unsafe(fn) || is_component_unsafe(prb))
		return NULL;

	if (asprintf(&ret, "%s:%s:%s:%s", prov, mod, fn, prb) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory making probespec\n");
		return NULL;
	}
	return ret;
}

/*
 * Split the pieces back up again.  The input spec becomes the provider name.
 * The pieces' lifetime is no longer than that of the input string.
 */
static int
split_probespec_name(char *spec, const char **mod, const char **fn,
		     const char **prb)
{
	char *tmp;

	tmp = strchr(spec, ':');
	if (tmp == NULL)
		return -1;
	*tmp = 0;
	*mod = tmp + 1;

	tmp = strchr(*mod, ':');
	if (tmp == NULL)
		return -1;
	*tmp = 0;
	*fn = tmp + 1;

	tmp = strchr(*fn, ':');
	if (tmp == NULL)
		return -1;
	*tmp = 0;
	*prb = tmp + 1;

	return 0;
}

/*
 * Compose a DOF directory's name from pieces.
 */
static char *
make_dof_name(dev_t dev, ino_t ino)
{
	char *ret;

	if (asprintf(&ret, "%" PRIuMAX "-%" PRIuMAX,
		     (uintmax_t)dev, (uintmax_t)ino) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory making DOF name\n");
		return NULL;
	}
	return ret;
}

/*
 * Split the pieces back up again.
 */
static int
split_dof_name(const char *dof_name, dev_t *dev, ino_t *ino)
{
	uintmax_t	dev_val, ino_val;
	dev_t		dev_tmp;
	ino_t		ino_tmp;
	char		*end, *ino_start;

	if (dof_name[0] < '0' || dof_name[0] > '9')
		goto err;

	errno = 0;
	dev_val = strtoumax(dof_name, &end, 10);
	if (errno != 0 || end == dof_name || *end != '-')
		goto err;

	ino_start = end + 1;
	if (*ino_start < '0' || *ino_start > '9')
		goto err;

	errno = 0;
	ino_val = strtoumax(ino_start, &end, 10);
	if (errno != 0 || end == ino_start || *end != '\0')
		goto err;

	dev_tmp = (dev_t)dev_val;
	ino_tmp = (ino_t)ino_val;
	if ((uintmax_t)dev_tmp != dev_val || (uintmax_t)ino_tmp != ino_val)
		goto err;

	*dev = dev_tmp;
	*ino = ino_tmp;
	return 0;

err:
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot derive dev/ino from %s\n",
			 dof_name);
	return -1;
}

/*
 * A trivial wrapper.
 */
static char *
make_numeric_name(long num)
{
	char *ret;

	if (asprintf(&ret, "%li", num) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory making filename\n");
		return NULL;
	}
	return ret;
}


/*
 * Push a piece of parsed DOF into a list to be emitted by
 * dof_stash_write_parsed().
 */
int
dof_stash_push_parsed(dt_list_t *accum, dof_parsed_t *parsed)
{
	dof_parsed_list_t *entry;

	if ((entry = calloc(1, sizeof(dof_parsed_list_t))) == NULL)
		return -1;

	entry->parsed = parsed;
	dt_list_append(accum, entry);

	return 0;
}

/*
 * Free all the accumulated parsed DOF in the passed list.
 */
void
dof_stash_free(dt_list_t *accum)
{
	dof_parsed_list_t *accump;
	dof_parsed_list_t *next = NULL;

	for (accump = dt_list_next(accum); accump != NULL; accump = next) {
		next = dt_list_next(accump);
		dt_list_delete(accum, accump);

		/*
		 * All parsed memory regions are terminated by an EOF, so once
		 * we encounter the EOF, this region can safely be freed.
		 */
		free(accump->parsed);
		free(accump);
	}
}

/*
 * Write a chunk of data to an fd.
 */
static int
write_chunk(int fd, const void *buf, size_t size)
{
	char *bufptr = (char *) buf;

	while (size > 0) {
		ssize_t len;

		if ((len = write(fd, bufptr, size)) < 0) {
			if (errno != EINTR)
				return -1;
			else
				continue;
		}

		size -= len;
		bufptr += len;
	}
	return 0;
}

/*
 * Prototype for utility function for usdt_data_apply().  Arguments are:
 *   - Directory fd
 *   - Base filename
 *   - USDT data
 *   - 'all' flag
 *   - Secondary directory fd
 */
typedef int (*usdt_data_fn)(int, const char *, const usdt_data_t *, int, int);

/*
 * Call the given function for every data block in the USDT data.  Each call
 * will be provided with the appropriate adjusted filename.
 *
 * If all == 0, returns the number of calls that returned true.
 * If all == 1, returns 0 if any of the calls did not return true, and
 * otherwise the total number of calls (that all returned true).
 * Return -1 if the operation fails.
 */
static int
usdt_data_apply(usdt_data_fn func, int dirfd, const char *name,
		const usdt_data_t *data, int all, int dirfd2)
{
	int i, cnt;
	char *fn = (char *)name;

	for (i = cnt = 0; data != NULL; i++, data = data->next) {
		if (i > 0) {
			if (asprintf(&fn, "%s-%d", name, i) < 0) {
				fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory making part name\n");
				return -1;
			}
		}

		switch (func(dirfd, fn, data, i, dirfd2)) {
		case 1:
			cnt++;
			break;
		case 0:
			break;
		case -1:
			return -1;
		}

		if (i > 0)
			free(fn);
	}

	if (all)
		return cnt == i ? 1 : 0;

	return cnt;
}

/*
 * Utility function used through usdt_data_apply().  This returns 1 if the file
 * is considered identical to the data block; otherwise 0.
 *
 * Note: if USDT data exists and has the same size, it is deemed identical.
 * (This is only a crude check -- distinct raw USDT data could be the same size
 * by pure chance.)
 */
static int
stale_data_file(int dirfd, const char *fn, const usdt_data_t *data, int idx,
		int dummy)
{
	struct stat s;

	if (fstatat(dirfd, fn, &s, 0) == 0)
		return s.st_size == data->size ? 1 : 0;

	return 0;
}

/*
 * Utility function used through usdt_data_apply().  Writes a USDT data block
 * as raw data to disk.  Returns 1 on success; -1 on failure.
 */
static int
write_raw_data(int dirfd, const char *fn, const usdt_data_t *data, int idx,
	       int dummy)
{
	int fd;

	if ((fd = openat(dirfd, fn, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		fuse_log(FUSE_LOG_ERR,
			 "dtprobed: cannot open raw data %s: %s\n",
			 fn, strerror(errno));
		return 0;
	}
	if (write_chunk(fd, &data->base, sizeof(size_t)) < 0 ||
	    write_chunk(fd, data->buf, data->size) < 0) {
		fuse_log(FUSE_LOG_ERR,
			 "dtprobed: cannot write out raw data %s: %s\n",
			 fn, strerror(errno));
		close(fd);
		return 0;
	}
	if (close(fd) < 0) {
		fuse_log(FUSE_LOG_ERR,
			 "dtprobed: cannot close raw data %s: %s\n",
			 fn, strerror(errno));
		return 0;
	}

	return 1;
}

/*
 * Utility function used through usdt_data_apply().  Removes the given file,
 * and returns 1 (failures can be ignored - nothing we can do about them).
 */
static int
remove_data_file(int dirfd, const char *fn, const usdt_data_t *data, int idx,
		 int dummy)
{
	unlinkat(dirfd, fn, 0);

	return 1;
}

/*
 * Write out a piece of raw USDT definition data.  Returns a positive integer
 * (the number of files written) if data was written, or 0 if none was needed
 * (or -1 on error).
 */
static int
dof_stash_write_raw(int dirfd, const char *name, const usdt_data_t *data)
{
	int rc;

	/*
	 * Verify whether the raw USDT data already exists on disk.  The return
	 * value will be 1 if it exists and all blocks are deemed identical to
	 * the USDT data; otherwise 0.  (Errors result in returning -1.)
	 */
	rc = usdt_data_apply(stale_data_file, dirfd, name, data, 1, 0);
	if (rc == -1)
		return -1;

	if (rc == 1)
		return 0;

	/*
	 * Write out the USDT data blocks.
	 *
	 *   rc = 1: All blocks written successfully.
	 *   rc = 0: Some blocks not written.
	 *   rc = -1: An error happened (some blocks not written).
	 *
	 * If some blocks were not written, we try to clean up (remove all that
	 * was written), and return -1.
	 */
	rc = usdt_data_apply(write_raw_data, dirfd, name, data, 1, 0);
	if (rc != 0)
		return rc;

	usdt_data_apply(remove_data_file, dirfd, name, data, 0, 0);
	return -1;
}

/*
 * Write out all the parsed DOF from the passed-in accumulator.  Split out from
 * dof_stash_add() because we don't want to create anything in the DOF stash for
 * a given piece of DOF until we have verified that all the parsed data for all
 * the probes in that DOF is valid.
 *
 * Each parsed file is a reduced parser stream (see the comments at the top of
 * this file), starting with a single 64-bit word giving the version of
 * dof_parsed_t in use, to ensure that readers never use an incompatible
 * version.
 *
 * We also write the parsed version out to the parsed/version file in the parsed
 * mapping dir, to make it easier to determine whether to reparse all the DOF
 * for outdatedness.
 */
int
dof_stash_write_parsed(pid_t pid, dev_t dev, ino_t ino, dt_list_t *accum)
{
	char *pid_name = make_numeric_name(pid);
	char *dof_name = make_dof_name(dev, ino);
	char *provpid_name = NULL;
	int perpid_dir, perpid_dof_dir, probepid_dir, parsed_mapping_dir,
		prov_dir = -1, mod_dir = -1, fun_dir = -1;
	dof_parsed_list_t *accump;
	const dof_parsed_t *this_provider = NULL;
	int version_fd, parsed_fd = -1;
	const char *op;
	const char *probe_err = "unknown probe";
	uint64_t version = DOF_PARSED_VERSION;
	int state = -1;
	int err;

	if (dt_list_next(accum) == NULL)
		goto early_out;

	if ((perpid_dir = openat(pid_dir, pid_name, O_PATH | O_CLOEXEC)) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: PID %i, %lx/%lx: open failed for PID dir for parsed DOF write: %s\n",
			 pid, dev, ino, strerror(errno));
		goto early_err;
	}

	op = "per-mapping open";
	if ((perpid_dof_dir = openat(perpid_dir, dof_name, O_PATH | O_CLOEXEC)) < 0)
		goto close_perpid_err;

	op = "per-mapping parsed DOF mkdirat";
	if ((parsed_mapping_dir = make_state_dirat(perpid_dof_dir, "parsed", op, 0)) < 0)
		goto close_perpid_dof_err;

	op = "probe PID mkdirat";
	if ((probepid_dir = make_state_dirat(probes_dir, pid_name, op, 0)) < 0)
		goto remove_parsed_mapping_err;

	if ((version_fd = openat(parsed_mapping_dir, "version",
				 O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, 0644)) < 0) {
		/*
		 * This creation failing indicates that this mapping already
		 * exists, i.e. it exists multiple times within the same
		 * process. ELF constructors will be run repeatedly
		 * in such cases, but we're only interested in the first.
		 *
		 * Note: in the future we may want to track this so that you
		 * need to do two DTRACEHIOC_REMOVEs to delete such things.)
		 */
		if (errno == EEXIST) {
			close(probepid_dir);
			close(parsed_mapping_dir);
			close(perpid_dof_dir);
			close(perpid_dir);
			free(pid_name);
			free(dof_name);
			return 0;
		}
		goto remove_probepid_dir_err;
	}

	if (write_chunk(version_fd, &version, sizeof(uint64_t)) < 0) {
		close(version_fd);
		goto remove_probepid_dir_err;
	}
	close(version_fd);

	/*
	 * We can just use assert() to check for overruns here, because any
	 * overruns are not corrupted input but a seriously buggy parser.
	 *
	 * On error, from this point on, we remove as little as possible on
	 * error, just a single probe's worth: better some probes survive an
	 * error condition than none do.
	 */
	for (accump = dt_list_next(accum); accump != NULL;
	     accump = dt_list_next(accump)) {
		char *mod, *fun, *prb;
		char *parsedfn = NULL;

		switch (accump->parsed->type) {
		/*
		 * Provider: make new provider dir.
		 */
		case DIT_PROVIDER: {
			state = accump->parsed->type;

			free(provpid_name);

			op = "provider name creation";
			this_provider = accump->parsed;
			provpid_name = make_provpid_name(this_provider->provider.name, pid);
			if (!provpid_name)
				goto err_provider;

			probe_err = this_provider->provider.name;

			op = "parsed provider close";
			if (maybe_close(prov_dir) < 0)
				goto err_provider;

			op = "probe provider mkdirat";
			if ((prov_dir = make_state_dirat(probepid_dir, provpid_name,
							 op, 1)) < 0)
				goto err_provider;
			break;
		}
		/*
		 * Probe: make (mod, fun, probe) dirs as needed: close
		 * out file, open new one, make hardlink to it.
		 */
		case DIT_PROBE: {
			assert(state == DIT_PROVIDER || state == DIT_PROBE ||
			       state == DIT_TRACEPOINT);
			state = accump->parsed->type;

			/*
			 * This rather weird pattern carries out all closes and
			 * then errors if any of them fail.
			 */
			op = "parsed probe close";
			err = maybe_close(mod_dir);
			err |= maybe_close(fun_dir);
			err |= maybe_close(parsed_fd);

			if (err != 0)
				goto err_provider;

			mod = accump->parsed->probe.name;
			assert(accump->parsed->size > (mod - (char *) accump->parsed));
			fun = mod + strlen(mod) + 1;
			assert(accump->parsed->size > (fun - (char *) accump->parsed));
			prb = fun + strlen(fun) + 1;
			assert(accump->parsed->size >= (prb - (char *) accump->parsed));

			op = "probe name construction";
			probe_err = this_provider->provider.name;
			if ((parsedfn = make_probespec_name(this_provider->provider.name,
							    mod, fun, prb)) == NULL)
				goto err_provider;

			op = "probe module";

			if ((mod_dir = make_state_dirat(prov_dir, mod, op, 0)) < 0)
				goto err_provider;

			op = "probe fun";
			if ((fun_dir = make_state_dirat(mod_dir, fun, op, 0)) < 0)
				goto err_probemod;

			op = "parsed probe creation";
			if ((parsed_fd = openat(parsed_mapping_dir, parsedfn,
					 O_CREAT | O_EXCL | O_WRONLY, 0644)) < 0)
				goto err_probefn;

			op = "parsed probe hardlink";
			errno = 0;
			if (linkat(parsed_mapping_dir, parsedfn, fun_dir, prb, 0) < 0
				&& errno != EEXIST)
				goto err_parsed_probe;

			op = "parsed version write";
			if (write_chunk(parsed_fd, &version, sizeof(uint64_t)) < 0)
				goto err_probe_link;

			op = "parsed provider write";
			if (write_chunk(parsed_fd, this_provider,
					this_provider->size) < 0)
				goto err_probe_link;

			op = "parsed probe write";
			if (write_chunk(parsed_fd, accump->parsed,
					accump->parsed->size) < 0)
				goto err_probe_link;

			break;
		}

		/* Args info or tracepoint: add to already-open file.  */
		case DIT_ARGS_NATIVE:
		case DIT_ARGS_XLAT:
		case DIT_ARGS_MAP:
		case DIT_TRACEPOINT:
			assert(state == DIT_PROBE || state == DIT_ARGS_NATIVE ||
			       state == DIT_ARGS_XLAT || state == DIT_ARGS_MAP ||
			       state == DIT_TRACEPOINT);
			state = accump->parsed->type;

			if (write_chunk(parsed_fd, accump->parsed, accump->parsed->size) < 0)
				goto err_provider_close;
			break;

		/* Error return from parser.  */
		case DIT_ERR:
			op = "parsing";
			fuse_log(FUSE_LOG_ERR, "dtprobed: parser error: %s\n",
				 accump->parsed->err.err);
			goto err_provider_close;
		/* EOF: nothing to do.  No need to record this parser-stream
		 * implementation detail into the parsed representation.  */
		case DIT_EOF:
			break;

		default:
			/*
			 * This is a parser bug, and a bad one.  Just fail
			 * hard.
			 */
			fuse_log(FUSE_LOG_ERR, "dtprobed: PID %i, %lx/%lx: internal error: corrupt parsed DOF: type %i\n",
				 pid, dev, ino, accump->parsed->type);
			abort();
		}

		free(parsedfn);
		continue;

	err_probe_link:
		unlinkat(fun_dir, prb, 0);
	err_parsed_probe:
		unlinkat(parsed_mapping_dir, parsedfn, 0);
	err_probefn:
		unlinkat(mod_dir, fun, AT_REMOVEDIR);
	err_probemod:
		if (prov_dir >= 0) /* Always true: placate compiler */
			unlinkat(prov_dir, mod, AT_REMOVEDIR);
	err_provider_close:
		maybe_close(parsed_fd);
		maybe_close(fun_dir);
		maybe_close(mod_dir);
	err_provider:
		maybe_close(prov_dir);
		unlinkat(probepid_dir, provpid_name, AT_REMOVEDIR);
		fuse_log(FUSE_LOG_ERR, "dtprobed: PID %i, %lx/%lx: %s failed for parsed DOF write for %s: %s\n",
			 pid, dev, ino, op, probe_err, strerror(errno));
		free(parsedfn);
		goto err_free;
	}

	err = maybe_close(prov_dir);
	err |= maybe_close(mod_dir);
	err |= maybe_close(fun_dir);
	err |= maybe_close(parsed_fd);

	if (err != 0) {
		unlinkat(perpid_dof_dir, "parsed", 0);
		goto err_free;
	}

	close(probepid_dir);
	close(parsed_mapping_dir);
	close(perpid_dof_dir);
	close(perpid_dir);
	free(provpid_name);

early_out:
	free(pid_name);
	free(dof_name);
	return 0;

	/* Errors requiring freeing and error return but no removals.  */
err_free:
	close(probepid_dir);
	close(parsed_mapping_dir);
	close(perpid_dof_dir);
	close(perpid_dir);
	close(prov_dir);
	free(provpid_name);
	free(pid_name);
	free(dof_name);
	return -1;

	/*
	 * Errors before parsed-probe creation starts require wholesale
	 * removals of stale dirs.
	 */

remove_probepid_dir_err:
	close(probepid_dir);
	unlinkat(probes_dir, pid_name, AT_REMOVEDIR);
	unlinkat(parsed_mapping_dir, "version", 0);
remove_parsed_mapping_err:
	close(parsed_mapping_dir);
	unlinkat(perpid_dof_dir, "parsed", AT_REMOVEDIR);
close_perpid_dof_err:
	close(perpid_dof_dir);
close_perpid_err:
	fuse_log(FUSE_LOG_ERR, "dtprobed: PID %i, %lx/%lx: %s failed for parsed DOF write: %s\n",
		 pid, dev, ino, op, strerror(errno));
	close(perpid_dir);
early_err:
	free(pid_name);
	free(dof_name);
	return -1;
}

/*
 * Read a file into a buffer and return it.  If READ_SIZE is non-negative, read
 * only that many bytes (and silently fail if the file isn't at least that
 * long).
 */
static void *
read_file(int fd, ssize_t read_size, size_t *size)
{
	struct stat s;
	char *buf;
	char *bufptr;
	int len;

	if (fstat(fd, &s) < 0) {
		fuse_log(FUSE_LOG_ERR, "cannot stat: %s\n", strerror(errno));
		return NULL;
	}
	if (read_size >= 0) {
		if (s.st_size < read_size)
			return NULL;
	} else
		read_size = s.st_size;

	if ((buf = malloc(read_size)) == NULL) {
		fuse_log(FUSE_LOG_ERR, "out of memory allocating %zi bytes\n",
			 read_size);
		return NULL;
	}

	if (size)
		*size = read_size;

	bufptr = buf;
	while ((len = read(fd, bufptr, read_size)) < read_size) {
		if (len < 0) {
			if (errno != EINTR) {
				fuse_log(FUSE_LOG_ERR, "cannot read: %s",
					 strerror(errno));
				free(buf);
				return NULL;
			}
			continue;
		}
		read_size -= len;
		bufptr += len;
	}
	return buf;
}

/*
 * Write out a buffer into a file. A trivial wrapper.
 *
 * If exists_ok is set, just do nothing if the file already exists.
 */
static
int dof_stash_write_file(int dirfd, const char *name, const void *buf,
			 size_t bufsiz, int exists_ok)
{
	int fd;

	errno = 0;
	if ((fd = openat(dirfd, name, O_CREAT | O_EXCL | O_WRONLY, 0644)) < 0) {
		if (exists_ok && errno == EEXIST)
			return 0;
		goto err;
	}

	if (write_chunk(fd, buf, bufsiz) < 0) {
		close(fd);
		goto err;
	}

	if (close(fd) < 0)
		goto err;

	return 0;

err:
	if (errno != EEXIST)
		unlinkat(dirfd, name, 0);

        fuse_log(FUSE_LOG_ERR, "dtprobed: cannot write out %s: %s\n", name,
		 strerror(errno));
	return -1;
}

/*
 * Figure out if a PID's entry in the DOF stash has been invalidated by an
 * exec(), by comparing the passed-in (dev, ino) against that recorded in
 * the exec-mapping.  Having no exec-mapping is perfectly valid and
 * indicates that no DOF has previously been seen for this PID at all.
 */
static int
dof_stash_execed(pid_t pid, int perpid_dir, dev_t dev, ino_t ino)
{
	char *exec_mapping;
	size_t size;
	int fd, rc;
	dev_t old_dev;
	ino_t old_ino;

	if ((fd = openat(perpid_dir, "exec-mapping", O_RDONLY)) < 0) {
		if (errno == ENOENT)
			return 0;
		goto err;
	}

	exec_mapping = read_file(fd, -1, &size);
	close(fd);
	if (exec_mapping == NULL)
		goto err;
	{
		char *nul, *tmp;

		nul = memchr(exec_mapping, '\0', size);
		if (nul != NULL && nul != exec_mapping + size - 1) {
			free(exec_mapping);
			goto err;
		}

		/*
		 * Current exec-mapping files include a terminating NUL, but
		 * older files do not.  Always append a terminator so the parser
		 * can treat both forms as C strings.
		 */
		if (nul == NULL) {
			tmp = realloc(exec_mapping, size + 1);
			if (tmp == NULL) {
				free(exec_mapping);
				goto err;
			}

			exec_mapping = tmp;
			exec_mapping[size] = '\0';
		}
	}

	rc = split_dof_name(exec_mapping, &old_dev, &old_ino);
	free(exec_mapping);
	if (rc < 0)
		goto err;

	return !((dev == old_dev) && (ino == old_ino));

err:
	fuse_log(FUSE_LOG_ERR, "Cannot determine if PID %i has execed; assuming not: %s\n",
		 pid, strerror(errno));
	return 0;
}

/*
 * Utility function used through usdt_data_apply().  Create a link to the given
 * file as raw-%n and return 1; return 0 on failure.
 */
static int
create_raw_link(int dirfd, const char *fn, const usdt_data_t *data, int idx,
		int rdirfd)
{
	char *rn = "raw";
	int rc;

	if (idx > 0) {
		if (asprintf(&rn, "raw-%d", idx) < 0) {
			fuse_log(FUSE_LOG_ERR,
				 "dtprobed: out of memory making part name\n");
			return -1;
		}
	}

	rc = linkat(dirfd, fn, rdirfd, rn, 0);

	if (idx > 0)
		free(rn);

	return rc < 0 ? 0 : 1;
}

/*
 * Add a piece of raw DOF from a given (pid, dev, ino) triplet.  May remove
 * stale DOF in the process.
 *
 * Return the new DOF generation number, or -1 on error.
 */
int
dof_stash_add(pid_t pid, dev_t dev, ino_t ino, dev_t exec_dev, dev_t exec_ino,
	      const dof_helper_t *dh, const usdt_data_t *data)
{
	char *dof_name = make_dof_name(dev, ino);
	char *pid_name = make_numeric_name(pid);
	int perpid_dir = -1;
	int perpid_dof_dir = -1;
	int gen_fd = -1;
	struct stat gen_stat;
	char *gen_name = NULL;
	int new_pid = 1;
	int new_dof = 0;
	int err = -1;

	if (!pid_name || !dof_name) {
		fuse_log(FUSE_LOG_ERR, "Out of memory stashing DOF\n");
		goto out_free;
	}

	perpid_dir = make_state_dirat(pid_dir, pid_name, "PID", 0);

	if (perpid_dir < 0)
		goto out_free;

	/* Figure out if the executable mapping has changed: if it has, purge
	   the entire PID and recreate it.  */

	if (dof_stash_execed(pid, perpid_dir, exec_dev, exec_ino)) {
		fuse_log(FUSE_LOG_DEBUG, "%i: exec() detected, removing old DOF\n", pid);
		if (dof_stash_remove_pid(pid) < 0) {
			fuse_log(FUSE_LOG_ERR, "PID %i exec()ed, but cannot remove dead DOF\n",
				 pid);
			goto err_unlink_nomsg;
		}

		perpid_dir = make_state_dirat(pid_dir, pid_name, "PID", 0);

		if (perpid_dir < 0)
			goto out_free;
	}

	perpid_dof_dir = make_state_dirat(perpid_dir, dof_name, "per-pid DOF", 0);

	if (perpid_dof_dir < 0)
		goto err_unlink_nomsg;

	fuse_log(FUSE_LOG_DEBUG, "%i: adding DOF in mapping %lx/%lx, l_addr %lx\n",
		 pid, dev, ino, dh->dofhp_addr);

	/* Get the current generation counter. */

	if ((gen_fd = openat(perpid_dir, "next-gen", O_CREAT | O_WRONLY, 0644)) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot open DOF generation counter for PID "
			 "%i: %s\n", pid, strerror(errno));
		goto err_unlink_nomsg;
	}

	if (fstat(gen_fd, &gen_stat) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot stat DOF generation counter for PID "
			 "%i: %s\n", pid, strerror(errno));
		goto err_unlink_nomsg;
	}
	if (gen_stat.st_size > 0)
		new_pid = 0;

	/*
	 * Write out the raw DOF, if not already known, and link it into its
	 * per-PID dir: write the helper directly out.  dof_stash_write_raw
	 * tells us whether the DOF actually needed writing or not (via a
	 * straight size comparison, which suffices to spot almost everything
	 * other than the pathological case of stale DOF from a killed process
	 * whose inode is unlinked and replaced with another identically-
	 * numbered inode with DOF of the *exact same length*: this seems too
	 * unlikely to be worth bothering with.)
	 *
	 * dof_stash_write_raw() returns 0 if no DOF needed writing because it
	 * already existed, -1 if there was an error, and a positive size
	 * otherwise.
	 */
	new_dof = 1;
	switch (dof_stash_write_raw(dof_dir, dof_name, data)) {
	case 0: new_dof = 0; break;
	case -1: goto err_unlink_nomsg; break;
	}

	if (usdt_data_apply(create_raw_link, dof_dir, dof_name, data, 1,
			    perpid_dof_dir) == 0)
		goto err_unlink_msg;

	if (dof_stash_write_file(perpid_dof_dir, "dh", dh,
				 sizeof(dof_helper_t), 0) < 0)
		goto err_unlink_nomsg;

	if (new_dof) {
		char *exec_mapping = make_dof_name(exec_dev, exec_ino);

		if (!exec_mapping) {
			fuse_log(FUSE_LOG_ERR, "Out of memory stashing new DOF\n");
			goto err_unlink_nomsg;
		}

		/*
		 * The exec-mapping wants writing out iff this is the first new
		 * DOF written for this PID.  We already checked for exec()
		 * above, so if exec-mapping still exists, its content must be
		 * identical to what we're about to write out: so treat an
		 * already-existing file as a do-nothing condition.
		 */
		if (dof_stash_write_file(perpid_dir, "exec-mapping", exec_mapping,
					 strlen(exec_mapping) + 1, 1) < 0) {
			free(exec_mapping);
			goto err_unlink_nomsg;
		}

		free(exec_mapping);
	}

	/*
	 * Update the generation counter and mark this DOF's generation.  On
	 * error after this point we leak generation counter values.
	 */
	if(ftruncate(gen_fd, gen_stat.st_size + 1) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot update generation counter for PID %i: %s\n",
			 pid, strerror(errno));
		goto err_unlink_nomsg;
	}

	gen_name = make_numeric_name(gen_stat.st_size);
	if (symlinkat(dof_name, perpid_dir, gen_name) < 0)
		goto err_unlink_msg;

	err = 0;

out_free:
	free(dof_name);
	free(pid_name);

	maybe_close(perpid_dir);
	maybe_close(perpid_dof_dir);
	maybe_close(gen_fd);

	return err == 0 ? gen_stat.st_size : err;

err_unlink_msg:
	fuse_log(FUSE_LOG_ERR, "Cannot link pids for DOF from PID %i, "
		 "DOF mapping %lx/%lx into place: %s\n", pid, dev, ino,
		 strerror(errno));
err_unlink_nomsg:
	usdt_data_apply(remove_data_file, perpid_dof_dir, "raw", data, 0, 0);
	unlinkat(perpid_dir, dof_name, AT_REMOVEDIR);

	if (gen_name)
		unlinkat(perpid_dir, gen_name, 0);

	if (new_pid) {
		unlinkat(perpid_dir, "next-gen", 0);
		unlinkat(pid_dir, pid_name, AT_REMOVEDIR);
	}

	if (new_dof) {
		usdt_data_apply(remove_data_file, dof_dir, dof_name, data, 0, 0);
		unlinkat(dof_dir, dof_name, 0);
	}

	goto out_free;
}

/*
 * The guts of unlinkat_many (below).  Returns -1 on error, or positive
 * ENOTEMPTY if a subdir deletion failed because the dir was nonempty (any
 * positive return leads to unwinding without doing any more deletions).
 */
static int
unlinkat_many_internal(int dirfd, int diroff, const char **names)
{
	int fd;
	struct stat s;
	int flags = 0;
	int ret;

	/*
	 * Open the dir/file at the next level down, then recurse to delete the
	 * next one in first.  This is a directory unless the next element in
	 * the list is NULL.
	 */

	if (names[diroff] == NULL)
		return 0;

	if (fstatat(dirfd, names[diroff], &s, AT_SYMLINK_NOFOLLOW) < 0) {
		if (errno == ENOENT)
			return 0;
		fuse_log(FUSE_LOG_ERR, "Cannot unlink %s during probe state dir deletion: %s\n",
			 names[diroff], strerror(errno));
		return -1;
	}

	if (S_ISDIR(s.st_mode))
		flags = AT_REMOVEDIR;

	if ((fd = openat(dirfd, names[diroff], O_RDONLY | O_CLOEXEC)) < 0) {
		fuse_log(FUSE_LOG_ERR, "Cannot open %s during probe state dir deletion: %s\n",
			 names[diroff], strerror(errno));
		return -1;
	}

	ret = unlinkat_many_internal(fd, diroff + 1, names);
	close(fd);

	if (ret < 0)
		return -1;
	else if (ret > 0)
		return 0;			/* ENOTEMPTY */

	/*
	 * Now do the deletion.
	 */
	if (unlinkat(dirfd, names[diroff], flags) < 0) {
		if (errno == ENOENT)
			return 0;
		if (errno == ENOTEMPTY)
			return ENOTEMPTY;
	}
	return 0;
}

/*
 * Act like rm -rd, removing a file and some number of empty directories above
 * it, as given by the names argument: the last element may be a directory or a
 * file.  Each directory in the names array is a subdirectory of the one before
 * it.  If any directory removal fails due to non-emptiness, all later removals
 * are skipped.  If any directory does not exist, removals start from that
 * point.
 */
static int
unlinkat_many(int dirfd, const char **names)
{
	return unlinkat_many_internal(dirfd, 0, names);
}

/*
 * Utility function used through usdt_data_apply().  Determine if a file in the
 * stash should be deleted, and if so, do it.  Returns 1 if it got deleted;
 * returns 0 if not needed; returns -1 on error.
 */
static int
refcount_cleanup_file(int dirfd, const char *fn, const usdt_data_t *data,
		      int idx, int dummy)
{
	struct stat s;

	if (fstatat(dirfd, fn, &s, 0) != 0) {
		if (errno == ENOENT) {
			((usdt_data_t *)data)->next = NULL;
			return 1;
		}

		fuse_log(FUSE_LOG_ERR, "Cannot stat %s for cleanup: %s\n",
			 fn, strerror(errno));
		return -1;
	}

	if (s.st_nlink != 1)
		return 0;

	if (unlinkat(dirfd, fn, 0) < 0) {
		fuse_log(FUSE_LOG_ERR,
			 "dtprobed: cannot remove old data %s: %s\n",
			 fn, strerror(errno));
		return -1;
	}

	return 1;
}

/*
 * Delete a directory (in the stash) if it has no other links.
 */
static int
refcount_cleanup_dir(int dirfd, const char *name)
{
	struct stat s;

	if (fstatat(dirfd, name, &s, 0) != 0) {
		fuse_log(FUSE_LOG_ERR, "Cannot stat %s for cleanup: %s\n",
			 name, strerror(errno));
		return -1;
	}

	if (s.st_nlink != 2)
		return 0;

	if (unlinkat(dirfd, name, AT_REMOVEDIR) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot remove old DOF %s: %s\n",
			 name, strerror(errno));
		return -1;
	}

	return 1;
}

/*
 * Remove the parsed directory under a given mapping directory.
 */
static int
dof_stash_remove_parsed(pid_t pid, int mapping_dir, const char *mapping)
{
	char *pid_name = make_numeric_name(pid);
	int parsed_dir;
	DIR *parsed;
	struct dirent *ent;

	if (((parsed_dir = openat(mapping_dir, "parsed", O_RDONLY | O_CLOEXEC)) < 0) ||
	    ((parsed = fdopendir(parsed_dir)) == NULL)) {

		if (parsed_dir >= 0)
			close(parsed_dir);

		if (errno == ENOENT)
			return 0;

		fuse_log(FUSE_LOG_ERR, "cannot open per-PID parsed DOF directory for PID %i mapping %s for cleanup: %s\n",
			 pid, mapping, strerror(errno));
		free(pid_name);
		return -1;
	}

	/*
	 * Scan the parsed dir, clean up all the matching per-probe
	 * dirs, and delete the parsed dir's contents too.
	 */

	while (errno = 0, (ent = readdir(parsed)) != NULL) {
		const char *mod, *fun, *prb;
		const char *unlink_err;
		char *prov = NULL, *provpid = NULL;

		if (errno != 0)
			goto err;

		if (ent->d_type != DT_REG)
			continue;

		if ((prov = strdup(ent->d_name)) == NULL)
			goto err;

		if (unlinkat(parsed_dir, ent->d_name, 0) < 0) {
			unlink_err = "parsed dir";
			goto next;
		}

		if (strcmp(ent->d_name, "version") == 0)
			continue;

		if (split_probespec_name(prov, &mod, &fun, &prb) < 0)
			continue;

		provpid = make_provpid_name(prov, pid);
		if (!provpid) {
			free(prov);
			goto err;
		}

		const char *dirnames[] = {pid_name, provpid, mod, fun, prb, NULL};
		if (unlinkat_many(probes_dir, dirnames) < 0) {
			unlink_err = "parsed probes dir entries";
			goto next;
		}
		free(prov);
		free(provpid);
		continue;

	  next:
		free(prov);
		free(provpid);
		fuse_log(FUSE_LOG_ERR, "cannot unlink per-PID %s %s (mapping %s) for cleanup: %s\n",
			 ent->d_name, unlink_err, mapping, strerror(errno));
	}
	closedir(parsed);
	free(pid_name);

	return 0;

err:
	closedir(parsed);
	free(pid_name);
	fuse_log(FUSE_LOG_ERR, "cannot open per-PID parsed DOF directory entry for PID %i mapping %s for cleanup: %s\n",
		 pid, mapping, strerror(errno));
	return -1;
}

/*
 * Utility function used through usdt_data_apply().  Removes the given file,
 * and returns 1 (failures can be ignored - nothing we can do about them).
 * If a file was successfully removed, data->next to set to data to signal the
 * iterator to move on to the next file.  If no file was found, data->next is
 * set to NULL to indicate that we are done.
 */
static int
remove_raw_file(int dirfd, const char *fn, const usdt_data_t *data, int idx,
		  int dummy)
{
	usdt_data_t *dp = (usdt_data_t *)data;

	if (unlinkat(dirfd, fn, 0) < 0 && errno == ENOENT)
		dp->next = NULL;
	else
		dp->next = dp;

	return 1;
}

/*
 * Remove a piece of DOF, identified by generation counter.  Return -1 on error.
 *
 * Not knowing about the DOF is not an error.
 */
int
dof_stash_remove(pid_t pid, int gen)
{
	char *pid_name = make_numeric_name(pid);
	char *gen_name = make_numeric_name(gen);
	char *gen_linkname = NULL;
	int perpid_dir = -1;
	int perpid_dof_dir = -1;
	struct stat pid_stat;
	struct stat gen_stat;
	int err = -1;
	const char *unlink_err = NULL;
	usdt_data_t data;

	/*
	 * Figure out the per-PID DOF directory by following the gen-counter
	 * symlink (by hand, because we want to reuse its name in the non-
	 * PID-specific dof/ subdir too).
	 */

	if (!pid_name || !gen_name)
		goto oom;

	if ((perpid_dir = openat(pid_dir, pid_name, O_PATH | O_CLOEXEC)) < 0)
		goto out;

	if (fstatat(perpid_dir, gen_name, &gen_stat, AT_SYMLINK_NOFOLLOW) < 0)
		goto out;

	if ((gen_linkname = malloc(gen_stat.st_size + 1)) == NULL)
		goto oom;

	/*
	 * Truncated dir -> concurrent modifications, not allowed.
	 */
	if (readlinkat(perpid_dir, gen_name, gen_linkname, gen_stat.st_size + 1) ==
	    gen_stat.st_size + 1) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: internal error: state dir is under concurrent modification, maybe multiple dtprobeds are running\n");
		exit(1);
	}
	gen_linkname[gen_stat.st_size] = 0;

	perpid_dof_dir = openat(perpid_dir, gen_linkname, O_PATH | O_CLOEXEC);

	if (strchr(gen_linkname, '/') != NULL ||
	    perpid_dof_dir < 0) {
		if (perpid_dof_dir >= 0)
			close(perpid_dof_dir);
		fuse_log(FUSE_LOG_ERR, "dtprobed: internal error: generation link for PID %i generation %i is %s, which is not valid\n",
			 pid, gen, gen_linkname);
		goto err;
	}

	if (faccessat(dof_dir, gen_linkname, F_OK, 0) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: internal error: generation link for PID %i generation %i is %s, which has no raw DOF\n",
			 pid, gen, gen_linkname);
		goto err;
	}

	/*
	 * Unlink the per-PID raw and parsed DOF and the probe dirs, then check
	 * the link counts on the non-per-PID stuff; if zero, unlink that too.
	 */

	fuse_log(FUSE_LOG_DEBUG, "%i: gen_name: %s; gen_linkname: %s; perpid_dof_dir: %i\n",
		 pid, gen_name, gen_linkname, perpid_dof_dir);

	/*
	 * We use a fake USDT data structure so we can use the USDT data block
	 * iterator to call our calllback.  It will keep iterating until the
	 * first non-existant file is encountered (indicating we reached the
	 * endof the data blocks).
	 */
	data.next = &data;
	usdt_data_apply(remove_raw_file, perpid_dof_dir, "raw", &data, 0, 0);

	if (dof_stash_remove_parsed(pid, perpid_dof_dir, gen_linkname) < 0)
		unlink_err = "parsed probes dir entries";

	if (unlinkat(perpid_dof_dir, "parsed", AT_REMOVEDIR) < 0)
		unlink_err = "parsed dir";

	if (unlinkat(perpid_dof_dir, "dh", 0) < 0)
		unlink_err = "dh";

	if (unlinkat(perpid_dir, gen_linkname, AT_REMOVEDIR) < 0)
		unlink_err = gen_linkname;

	if (unlinkat(perpid_dir, gen_name, 0) < 0)
		unlink_err = gen_name;

	/*
	 * We use a fake USDT data structure so we can use the USDT data block
	 * iterator to call our calllback.  It will keep iterating until the
	 * first non-existance file is encountered (indicating we reached the
	 * endof the data blocks).
	 */
	data.next = &data;
	if (usdt_data_apply(refcount_cleanup_file, dof_dir, gen_linkname,
			    &data, 0, 0) < 0)
		unlink_err = gen_linkname;

	/*
	 * Clean up the PID directory itself, if it is now empty.  We can't just
	 * use refcount_cleanup here because we also have to delete the
	 * generation counter right before deleting the directory.
	 */

	if (fstatat(pid_dir, pid_name, &pid_stat, 0) < 0) {
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot stat per-PID for pid %i: %s\n",
			 pid, strerror(errno));
		goto err;
	}

	if (pid_stat.st_nlink == 2) {
		if (unlinkat(perpid_dir, "next-gen", 0) < 0) {
			fuse_log(FUSE_LOG_ERR, "dtprobed: cannot clean up gen counter in per-PID dir for PID %i: %s\n",
				 pid, strerror(errno));
			goto err;
		}
		if (unlinkat(perpid_dir, "exec-mapping", 0) < 0
			&& errno != ENOENT) {
			fuse_log(FUSE_LOG_ERR, "dtprobed: cannot clean up exec-mapping in per-PID dir for PID %i: %s\n",
				 pid, strerror(errno));
			goto err;
		}
		refcount_cleanup_dir(pid_dir, pid_name);
	}

	if (unlink_err)
		fuse_log(FUSE_LOG_ERR, "dtprobed: cannot unlink per-PID DOF %s for PID %i generation %i: %s\n",
			 unlink_err, pid, gen, strerror(errno));

out:
	err = 0;

err:
	free(pid_name);
	free(gen_name);
	free(gen_linkname);

	maybe_close(perpid_dir);
	maybe_close(perpid_dof_dir);

	return err;

oom:
	fuse_log(FUSE_LOG_ERR, "dtprobed: out of memory unlinking DOF\n");
	goto err;
}

/*
 * Remove all DOF registered for a given PID.  Used when new DOF arrives and the
 * primary text mapping is found to be different from what it was (when exec()),
 * and when processes die without deregistering (see below).
 */
int
dof_stash_remove_pid(pid_t pid)
{
	char *pid_name = make_numeric_name(pid);
	struct dirent *ent;
	DIR *dir;
	int tmp;
	int err = -1;

	if ((tmp = openat(pid_dir, pid_name, O_RDONLY | O_CLOEXEC)) < 0) {
		fuse_log(FUSE_LOG_ERR, "cannot open per-PID DOF mappings directory for pid %s for cleanup: %s\n",
			 pid_name, strerror(errno));
		goto out;
	}

	if ((dir = fdopendir(tmp)) == NULL) {
		fuse_log(FUSE_LOG_ERR, "cannot clean up per-PID DOF mappings for PID %s: %s\n",
			 pid_name, strerror(errno));
		close(tmp);
		goto out;
	}

	fuse_log(FUSE_LOG_DEBUG, "pruning dead/execed PID %s\n", pid_name);

	/* Work over all the mappings in this PID. */

	while (errno = 0, (ent = readdir(dir)) != NULL) {
		int gen;
		char *end_gen;

		if (errno != 0) {
			fuse_log(FUSE_LOG_ERR, "cannot read per-PID DOF mappings for PID %i for cleanup: %s\n",
				 pid, strerror(errno));
			closedir(dir);
			goto out;
		}

		if (ent->d_type != DT_LNK)
			continue;

		fuse_log(FUSE_LOG_DEBUG, "Working over generation %s\n",
			 ent->d_name);

		gen = strtol(ent->d_name, &end_gen, 10);
		if (*end_gen != '\0')
			continue;

		if (dof_stash_remove(pid, gen) < 0) {
			fuse_log(FUSE_LOG_ERR, "cannot remove dead pid %i\n", pid);
			closedir(dir);
			goto out;
		}
	}
	closedir(dir);
	err = 0;

out:
	free(pid_name);
	return err;
}

/*
 * Prune dead processes out of the DOF stash.  This is not a correctness
 * operation, just a space-waste reducer. If a process with DOF died uncleanly
 * and its PID was recycled since a cleanup was last run, we'll leave some DOF
 * around for a bit, but there are no correctness implications: either the new
 * process has no DOF at all, in which case we'll never learn about it, or it
 * does, in which case either it has identical mappings (in which case we'll
 * just reuse them) or it has different ones (in which case we'll just add to
 * them): in neither case is there any negative consequence of the nonexistent
 * mappings other than a bit of space wastage.  (This implies that any process
 * reading DOF out of the state directory must check that a process that claims
 * to have DOF at a given mapping actually has a corresponding mapping with the
 * same dev/ino pair at the right address. There's no need for that process to
 * clean it up if found to be wrong, though: this function will eventually get
 * around to it.)
 */
void
dof_stash_prune_dead(void)
{
	DIR *dir;
	struct dirent *ent;
	int tmp;

	fuse_log(FUSE_LOG_DEBUG, "Pruning dead PIDs\n");

	if ((tmp = dup(pid_dir)) < 0) {
		fuse_log(FUSE_LOG_ERR, "cannot dup() for cleanup: %s\n", strerror(errno));
		return;
	}

	if ((dir = fdopendir(tmp)) == NULL) {
		close(tmp);
		fuse_log(FUSE_LOG_ERR, "cannot clean up per-PID DOF directory: %s\n",
			 strerror(errno));
		return;
	}
	rewinddir(dir);

	/*
	 * Work over all the PIDs.
	 */
	while (errno = 0, (ent = readdir(dir)) != NULL) {
		char *end_pid;
		pid_t pid;

		if (errno != 0)
			goto scan_failure;

		if (ent->d_type != DT_DIR)
			continue;

		/*
		 * Only directories with numeric names can be PIDs: skip all the
		 * rest.
		 */
		pid = strtol(ent->d_name, &end_pid, 10);
		if (*end_pid != '\0')
			continue;

		if (kill(pid, 0) == 0 || errno != ESRCH)
			continue;

		/*
		 * Process dead: clean up its PIDs, deleting everything in the
		 * per-PID directory by calling dof_stash_remove() on every
		 * mapping in turn.
		 */

		dof_stash_remove_pid(pid);
		errno = 0;
	}

	if (errno != 0)
		goto scan_failure;

out:
	closedir(dir);
	return;

scan_failure:
	fuse_log(FUSE_LOG_ERR, "cannot scan per-PID DOF directory for cleanup: %s\n",
		 strerror(errno));
	goto out;
}

/*
 * Utility function used through usdt_data_apply().  Read a USDT data block
 * from disk.  Returns 1 on success of if the file is not found; -1 on error.
 */
static int
read_raw_data(int dirfd, const char *fn, const usdt_data_t *data, int idx,
	      int dummy)
{
	int fd;
	usdt_data_t *dp = NULL;

	/*
	 * If the file does not exist, we assume that we have reached the last
	 * file for this USDT data.  We can return 1, because we know that
	 * data->next is NULL so the iterator will stop.
	 */
	if ((fd = openat(dirfd, fn, O_RDONLY | O_CLOEXEC)) < 0) {
		if (errno == ENOENT)
			return 1;

		return -1;
	}

	/*
	 * Allocate a new block.  We set the next pointer to the block itself
	 * so that the iterator that called us knows that we need look for a
	 * following block.  If none is found, the next call will assign NULL
	 * to this next pointer and end the block chain.
	 */
	if ((dp = malloc(sizeof(usdt_data_t))) == NULL)
		goto err;

	dp->size = 0;
	dp->base = 0;
	dp->next = NULL;

	if ((dp->buf = read_file(fd, -1, &dp->size)) == NULL)
		goto err;
	if (dp->size < sizeof(size_t)) {
		free(dp->buf);
		goto err;
	}

	close(fd);

	/*
	 * Raw data blocks are written as a base address (size_t) followed by
	 * the actual data.  Set dp->base from the data just read, and adjust
	 * the buffer pointer and size.  When the buffer is to be freed, the
	 * pointer will need to be adjusted back.
	 */
	dp->base = *(size_t *)dp->buf;
	dp->size -= sizeof(size_t);
	dp->buf = ((char *)dp->buf) + sizeof(size_t);

	((usdt_data_t *)data)->next = dp;
	return 1;

err:
	close(fd);
	free(dp);
	return -1;
}

/*
 * Reparse all DOF.  Mappings that cannot be reparsed are simply ignored, on the
 * grounds that most DOF, most of the time, is not used, so this will likely be
 * ignorable.
 *
 * Normally, DOF with the same DOF_VERSION as the currently running daemon is
 * left alone: if force is set, even that is reparsed.
 */
int
reparse_dof(int out, int in,
	    int (*reparse)(int pid, int out, int in, dev_t dev, ino_t ino,
			   dev_t unused1, ino_t unused2, dof_helper_t *dh,
			   const usdt_data_t *data, int reparsing),
	    int force)
{
	DIR *all_pids_dir;
	struct dirent *pid_ent;
	int err = -1;
	int tmp;

	if ((tmp = dup(pid_dir)) < 0) {
		fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot dup(): %s\n", strerror(errno));
		return -1;
	}

	if ((all_pids_dir = fdopendir(tmp)) == NULL) {
		close(tmp);
		fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot read per-PID DOF directory: %s\n",
			 strerror(errno));
		return -1;
	}

	/* Work over all the PIDs. */

	rewinddir(all_pids_dir);
	while (errno = 0, (pid_ent = readdir(all_pids_dir)) != NULL) {
		int perpid_fd;
		char *end_pid;
		pid_t pid;
		DIR *perpid_dir;
		struct dirent *mapping_ent;

		if (errno != 0)
			goto scan_failure;

		if (pid_ent->d_type != DT_DIR)
			continue;

		/*
		 * Only directories with numeric names can be PIDs: skip all the
		 * rest.
		 */
		pid = strtol(pid_ent->d_name, &end_pid, 10);
		if (*end_pid != '\0')
			continue;

		if ((perpid_fd = openat(pid_dir, pid_ent->d_name, O_RDONLY | O_CLOEXEC)) < 0) {
			fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot open per-PID DOF mappings directory for PID %s: %s\n",
				 pid_ent->d_name, strerror(errno));
			goto out;
		}

		if ((tmp = dup(perpid_fd)) < 0) {
			fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot dup(): %s\n", strerror(errno));
			close(perpid_fd);
			goto out;
		}

		if ((perpid_dir = fdopendir(tmp)) == NULL) {
			fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot read per-PID DOF mappings for pid %s: %s\n",
				 pid_ent->d_name, strerror(errno));
			close(tmp);
			close(perpid_fd);
			goto out;
		}

		/* Work over all the mappings in this PID. */

		while (errno = 0, (mapping_ent = readdir(perpid_dir)) != NULL) {
			int mapping_fd;
			int fd;
			dev_t dev;
			ino_t ino;
			size_t dh_size;
			void *dh = NULL;
			usdt_data_t data, *dp, *nxt;

			if (errno != 0) {
				fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot read per-PID DOF mappings for pid %s: %s\n",
					 pid_ent->d_name, strerror(errno));
				goto perpid_err;
			}

			if (mapping_ent->d_type != DT_DIR)
				continue;

			/*
			 * Hidden files cannot be mappings.  (This also skips
			 * . and ..).
			 */
			if (mapping_ent->d_name[0] == '.')
				continue;

			if ((mapping_fd = openat(perpid_fd, mapping_ent->d_name,
						 O_PATH | O_CLOEXEC)) < 0) {
				fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot open per-PID DOF mapping for PID %s, mapping %s: %s\n",
					 pid_ent->d_name, mapping_ent->d_name, strerror(errno));
				goto perpid_err;
			}

			fuse_log(FUSE_LOG_DEBUG, "Parsed DOF is present: evaluating\n");

			/*
			 * Only DOF with a different parsed representation gets
			 * reparsed.
			 */
			if (!force) {
				if ((fd = openat(mapping_fd, "parsed/version",
						 O_RDONLY | O_CLOEXEC)) >= 0) {
					uint64_t *parsed_version;

					parsed_version = read_file(fd, sizeof(uint64_t),
								   NULL);
					close(fd);

					if (parsed_version != NULL &&
					    *parsed_version == DOF_PARSED_VERSION) {
						fuse_log(FUSE_LOG_DEBUG, "No need to reparse\n");
						close(mapping_fd);
						continue;
					}

					free(parsed_version);
				}
			}

			fuse_log(FUSE_LOG_DEBUG, "Deleting\n");

			if (dof_stash_remove_parsed(pid, mapping_fd, mapping_ent->d_name) < 0) {
				fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot delete parsed DOF for PID %s, mapping %s: %s\n",
					 pid_ent->d_name, mapping_ent->d_name, strerror(errno));
				close(mapping_fd);
				goto perpid_err;
			}

			if (split_dof_name(mapping_ent->d_name, &dev, &ino) < 0) {
				close(mapping_fd);
				continue;
			}

			data.base = 0;
			data.size = 0;
			data.buf = NULL;
			data.next = NULL;
			if (usdt_data_apply(read_raw_data, mapping_fd, "raw",
					    &data, 0, 0) <= 0) {
				fuse_log(FUSE_LOG_ERR,
					 "reparse: cannot open raw data for PID %s, mapping %s: ignored: %s\n",
					 pid_ent->d_name, mapping_ent->d_name, strerror(errno));
				goto read_err;
			}

			if ((fd = openat(mapping_fd, "dh", O_RDONLY | O_CLOEXEC)) < 0) {
				fuse_log(FUSE_LOG_ERR, "when reparsing DOF, cannot open dh for PID %s, mapping %s: ignored: %s\n",
					    pid_ent->d_name, mapping_ent->d_name, strerror(errno));
				goto read_err;
			}

			if ((dh = read_file(fd, -1, &dh_size)) == NULL) {
				fuse_log(FUSE_LOG_ERR, "when reparsing DOF, cannot read dh for PID %s, mapping %s: ignored: %s\n",
					    pid_ent->d_name, mapping_ent->d_name, strerror(errno));
				close(fd);
				goto read_err;
			}
			close(fd);

			fuse_log(FUSE_LOG_DEBUG,
				 "Reparsing raw data for PID %s, mapping %s\n",
				 pid_ent->d_name, mapping_ent->d_name);

			if (reparse(pid, out, in, dev, ino, 0, 0,
				    dh, data.next, 1) < 0)
				fuse_log(FUSE_LOG_ERR,
					 "reparse: cannot parse raw data for PID %s, mapping %s: ignored\n",
					 pid_ent->d_name, mapping_ent->d_name);

			free(dh);

read_err:
			close(mapping_fd);

			for (dp = data.next; dp != NULL; dp = nxt) {
				nxt = dp->next;
				free(((char *)dp->buf) - sizeof(size_t));
				free(dp);
			}

			continue;

		perpid_err:
			closedir(perpid_dir);
			close(perpid_fd);
			goto out;
		}
		closedir(perpid_dir);
		close(perpid_fd);
		errno = 0;
	}

	if (errno != 0)
		goto scan_failure;

	err = 0;
out:
	closedir(all_pids_dir);
	return err;

scan_failure:
	fuse_log(FUSE_LOG_ERR, "reparsing DOF: cannot scan per-PID DOF directory: %s\n",
		 strerror(errno));
	goto out;
}
