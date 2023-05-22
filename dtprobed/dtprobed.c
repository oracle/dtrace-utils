/*
 * Oracle Linux DTrace; DOF-consumption and USDT-probe-creation daemon.
 * Copyright (c) 2022, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <config.h>

#include <linux/seccomp.h>
#include <sys/syscall.h>

/*
 * Compatibility.  With libfuse 3, we use a few newer features that need at
 * least version 31 (3.2.0): with libfuse 2, the default will suffice.  We also
 * use the logging infrastructure added in libfuse 3.7, but provide an alternate
 * implementation if not available.
 */

#if HAVE_LIBFUSE3
#define FUSE_USE_VERSION 31
#else /* libfuse 2 */
/* Use the default (21).  */
#endif

#include <cuse_lowlevel.h>
#include <fuse_lowlevel.h>
#ifdef HAVE_FUSE_LOG
#include <fuse_log.h>
#else
#include "rpl_fuse_log.h"
#endif
#include <port.h>

#include <dtrace/ioctl.h>

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "dof_parser.h"
#include "uprobes.h"

#include "seccomp-assistance.h"

#define DOF_MAXSZ 512 * 1024 * 1024
#define DOF_CHUNKSZ 64 * 1024

static struct fuse_session *cuse_session;

int _dtrace_debug = 0;
static int foreground;
void dt_debug_dump(int unused) {} 		/* For libproc.  */

static pid_t parser_pid;
static int parser_in_pipe[2];
static int parser_out_pipe[2];
static int timeout = 5000; 			/* In seconds.  */

static void helper_ioctl(fuse_req_t req, int cmd, void *arg,
			 struct fuse_file_info *fi, unsigned int flags,
			 const void *in_buf, size_t in_bufsz, size_t out_bufsz);

static int process_dof(fuse_req_t req, int out, int in, pid_t pid,
		       dof_helper_t *dh, const void *in_buf);

static const struct cuse_lowlevel_ops dtprobed_clop = {
	.ioctl = helper_ioctl,
};

static void
log_msg(enum fuse_log_level level, const char *fmt, va_list ap)
{
	if (!_dtrace_debug && level > FUSE_LOG_INFO)
		return;

	if (foreground)
		vfprintf(stderr, fmt, ap);
	else
		vsyslog(level, fmt, ap);
}

/* For libproc */
void
dt_debug_printf(const char *subsys, const char *fmt, va_list ap)
{
	if (!_dtrace_debug)
		return;

	if (foreground) {
		fprintf(stderr, "%s DEBUG: ", subsys);
		vfprintf(stderr, fmt, ap);
	} else {
		/* Subsystem discarded (it's always 'libproc' anyway).  */
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

#if HAVE_LIBFUSE3
static int
session_fd(struct fuse_session *cuse_session)
{
	return fuse_session_fd(cuse_session);
}
#else /* libfuse 2 */
static struct fuse_chan *cuse_chan;

static void
init_cuse_chan(struct fuse_session *cuse_session)
{
	cuse_chan = fuse_session_next_chan(cuse_session, NULL);
}

static int
session_fd(struct fuse_session *cuse_session)
{
	return fuse_chan_fd(cuse_chan);
}
#endif

/*
 * States for the ioctl processing loop, which gets repeatedly called due to the
 * request/reply nature of unrestricted FUSE ioctls.
 */
typedef enum dtprobed_fuse_state {
	DTP_IOCTL_START = 0,
	DTP_IOCTL_HDR = 1,
	DTP_IOCTL_DOFHDR = 2,
	DTP_IOCTL_DOFCHUNK = 3,
	DTP_IOCTL_DOF = 4
} dtprobed_fuse_state_t;

/*
 * State crossing calls to CUSE request functions.
 */
typedef struct dtprobed_userdata {
	dtprobed_fuse_state_t state;
	dof_helper_t dh;
	dof_hdr_t dof_hdr;
	size_t next_chunk;
	char *buf;
} dtprobed_userdata_t;

struct fuse_session *
setup_helper_device(int argc, char **argv, char *devname, dtprobed_userdata_t *userdata)
{
	struct cuse_info ci;
	struct fuse_session *cs;
	char *args;
	int multithreaded;

	memset(&ci, 0, sizeof(struct cuse_info));

	ci.flags = CUSE_UNRESTRICTED_IOCTL;
	ci.dev_info_argc = 1;
	if (asprintf(&args,"DEVNAME=%s", devname) < 0) {
		perror("allocating helper device");
		exit(2);			/* Allow restarting.  */
	}

	const char *dev_info_argv[] = { args };
	ci.dev_info_argv = dev_info_argv;

	cs = cuse_lowlevel_setup(argc, argv, &ci, &dtprobed_clop,
				 &multithreaded, userdata);

	if (cs == NULL) {
		perror("allocating helper device");
		return NULL;
	}

#ifndef HAVE_LIBFUSE3 /* libfuse 2 */
	init_cuse_chan(cs);
#endif

	if (multithreaded) {
		fprintf(stderr, "CUSE thinks dtprobed is multithreaded!\n");
		fprintf(stderr, "This should never happen.\n");
		errno = EINVAL;
		return NULL;
	}

	free(args);
	return cs;
}

void
teardown_device(void)
{
	/* This is automatically called on SIGTERM.  */
	cuse_lowlevel_teardown(cuse_session);
}

/*
 * Parse a piece of DOF.  Return 0 iff the pipe has closed and no more parsing
 * is possible.
 */
static int
parse_dof(int in, int out)
{
	int ok;
	dof_helper_t *dh;
	dof_hdr_t *dof;

	dh = dof_copyin_helper(in, out, &ok);
	if (!dh)
		return ok;

	dof = dof_copyin_dof(in, out, &ok);
	if (!dof) {
		free(dh);
		return ok;
	}

	dof_parse(out, dh, dof);

	return ok;
}

/*
 * Kick off the sandboxed DOF parser.  This is run in a seccomp()ed subprocess,
 * and sends a stream of dof_parsed_t back to this process.
 */
static void
dof_parser_start(int sync_fd)
{
	if ((pipe(parser_in_pipe) < 0) ||
	    (pipe(parser_out_pipe) < 0))
		daemon_perr(sync_fd, "cannot create DOF parser pipes", errno);

	switch (parser_pid = fork()) {
	case -1:
		daemon_perr(sync_fd, "cannot fork DOF parser", errno);
	case 0: {
		/*
		 * Sandboxed parser child.  Close unwanted fds and nail into
		 * seccomp jail.
		 */
		close(session_fd(cuse_session));
		close(parser_in_pipe[1]);
		close(parser_out_pipe[0]);
		if (!foreground)
			close(sync_fd);

		/*
		 * Reporting errors at this point is difficult: we have already
		 * closed all pipes that we might use to report it.  Just exit 1
		 * and rely on the admin using strace :(
		 *
		 * Take additional measures to ensure that we can still do
		 * sufficiently-large mallocs in the child without needing to
		 * make any syscalls.
		 *
		 * Don't lock the process in jail if debugging (but still run in
		 * a child process).
		 */
		mallopt(M_MMAP_MAX, 0);
		mallopt(M_TRIM_THRESHOLD, -1);
		seccomp_fill_memory_really_free(seccomp_fill_memory_really_malloc(DOF_MAXSZ * 2));

		if (!_dtrace_debug)
			if (syscall(SYS_seccomp, SECCOMP_SET_MODE_STRICT, 0, NULL) < 0)
				_exit(1);

		while (parse_dof(parser_in_pipe[0], parser_out_pipe[1]))
			;
		_exit(0);
	}
	}

	close(parser_in_pipe[0]);
	close(parser_out_pipe[1]);
}

/*
 * Clean up wreckage if the DOF parser dies: optionally restart it.
 */
static void
dof_parser_tidy(int restart)
{
	int status = 0;

	if (parser_pid == 0)
		return;

	kill(parser_pid, SIGKILL);
	if (errno != ESRCH)
		while (waitpid(parser_pid, &status, 0) < 0 && errno == EINTR);

	close(parser_in_pipe[1]);
	close(parser_out_pipe[0]);

	if (restart)
		dof_parser_start(-1);
}

static dof_parsed_t *
dof_read(fuse_req_t req, int in)
{
	dof_parsed_t *reply = dof_parser_host_read(in, timeout);

	if (!reply)
		return NULL;

	/*
	 * Log errors.
	 */
	if (reply->type == DIT_ERR) {
		errno = reply->err.err_no;
		fuse_log(FUSE_LOG_WARNING, "%i: dtprobed: DOF parsing error: "
			 "%s\n", fuse_req_ctx(req)->pid,
			 reply->err.err);
		free(reply);
		reply = NULL;
	}

	return reply;
}

/*
 * Create probes as requested by the dof_parsed_t parsed from the DOF.
 * The DOF parser has already applied the l_addr offset derived from the client
 * process's dynamic linker.
 */
static void
create_probe(pid_t pid, dof_parsed_t *provider, dof_parsed_t *probe,
    dof_parsed_t *tp)
{
	const char *mod, *fun, *prb;

	mod = probe->probe.name;
	fun = mod + strlen(mod) + 1;
	prb = fun + strlen(fun) + 1;

	free(uprobe_create_from_addr(pid, tp->tracepoint.addr,
		tp->tracepoint.is_enabled, provider->provider.name,
		mod, fun, prb));
}

/*
 * Core ioctl() helper.  Repeatedly reinvoked after calls to
 * fuse_reply_ioctl_retry, once per dereference.
 */
static void
helper_ioctl(fuse_req_t req, int cmd, void *arg,
	     struct fuse_file_info *fi, unsigned int flags,
	     const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	dtprobed_userdata_t *userdata = fuse_req_userdata(req);
	struct iovec in;
	pid_t pid = fuse_req_ctx(req)->pid;
	const char *errmsg;
	const void *buf;

	/*
	 * We can just ignore FUSE_IOCTL_COMPAT: the 32-bit and 64-bit versions
	 * of the DOF structures are intentionally identical.
	 */

	switch (cmd) {
	case DTRACEHIOC_ADDDOF:
		break;
	case DTRACEHIOC_REMOVE: /* TODO */
		fuse_reply_ioctl(req, 0, NULL, 0);
		return;
	default: errmsg = "invalid ioctl";;
		fuse_log(FUSE_LOG_WARNING, "%i: dtprobed: %s %lx\n",
			 pid, errmsg, cmd);
		goto fuse_err;
	}

	/*
	 * First call: get the ioctl arg content, a dof_helper_t.
	 */
	if (userdata->state == DTP_IOCTL_START) {
		in.iov_base = arg;
		in.iov_len = sizeof(dof_helper_t);

		errmsg = "error reading ioctl size";
		if (fuse_reply_ioctl_retry(req, &in, 1, NULL, 0) < 0)
			goto fuse_errmsg;

		/*
		 * userdata->buf should already be freed, but if somehow it is
		 * non-NULL make absolutely sure it is cleared out.
		 */
		userdata->buf = NULL;
		userdata->next_chunk = 0;
		userdata->state = DTP_IOCTL_HDR;
		return;
	}

	/*
	 * Second call: validate the dof_hdr_t length, get the initial DOF.
	 */
	if (userdata->state == DTP_IOCTL_HDR) {
		if (in_bufsz != sizeof(dof_helper_t)) {
			errmsg = "helper size incorrect";
			fuse_log(FUSE_LOG_ERR, "%i: dtprobed: %s: "
				 "expected at least %zi, not %zi\n", pid,
				 errmsg, sizeof(dof_helper_t), in_bufsz);
			goto fuse_err;
		}
		memcpy(&userdata->dh, in_buf, sizeof(dof_helper_t));

		in.iov_base = (void *) userdata->dh.dofhp_dof;
		in.iov_len = sizeof(dof_hdr_t);

		errmsg = "cannot read DOF header";
		if (fuse_reply_ioctl_retry(req, &in, 1, NULL, 0) < 0)
			goto fuse_errmsg;

		userdata->state = DTP_IOCTL_DOFHDR;
		return;
	}

	/*
	 * From here on we are always fetching DOF: the inbound buffer must be
	 * at least as big as the DOF header.
	 */
	if (in_bufsz < sizeof(dof_hdr_t)) {
		errmsg = "DOF too small";
		fuse_log(FUSE_LOG_ERR, "%i: dtprobed: %s: expected at least %zi, "
			 "not %zi\n", pid, errmsg, sizeof(dof_hdr_t), in_bufsz);
		goto fuse_err;
	}

	/*
	 * Third call: validate the DOF length and stash it away.
	 */
	if (userdata->state == DTP_IOCTL_DOFHDR) {
		/*
		 * Too much data is as bad as too little.
		 */
		if (userdata->state == DTP_IOCTL_DOFHDR &&
		    (in_bufsz > sizeof(dof_hdr_t))) {
			errmsg = "DOF header size incorrect";
			fuse_log(FUSE_LOG_ERR, "%i: dtprobed: %s: %zi, not %zi\n",
				 pid, errmsg, in_bufsz, sizeof(dof_hdr_t));
			goto fuse_err;
		}
		memcpy(&userdata->dof_hdr, in_buf, sizeof(dof_hdr_t));

		if (userdata->dof_hdr.dofh_loadsz > DOF_MAXSZ)
			fuse_log(FUSE_LOG_WARNING, "%i: dtprobed: DOF size of %zi longer than is sane\n",
				 pid, userdata->dof_hdr.dofh_loadsz);

		/* Fall through. */
	}

	/*
	 * Third and possibly further calls: get the DOF itself, in chunks if
	 * it's too big.
	 */
	if (userdata->state == DTP_IOCTL_DOFHDR ||
		userdata->state == DTP_IOCTL_DOFCHUNK) {
		int by_chunks = 0;

		in.iov_base = (void *) userdata->dh.dofhp_dof;
		in.iov_len = userdata->dof_hdr.dofh_loadsz;

		/*
		 * If the data being read in is too large, read by chunks.  We
		 * cannot determine the chunk size we can use: the first failure
		 * returns -ENOMEM to the caller of ioctl().
		 */
		if (userdata->dof_hdr.dofh_loadsz > DOF_CHUNKSZ) {
			by_chunks = 1;

			if (userdata->state == DTP_IOCTL_DOFHDR) {
				userdata->buf = malloc(userdata->dof_hdr.dofh_loadsz);
				in.iov_len = DOF_CHUNKSZ;

				if (userdata->buf == NULL) {
					errmsg = "out of memory allocating DOF";
					goto fuse_errmsg;
				}
			} else {
				ssize_t remaining;
				size_t to_copy;

				remaining = userdata->dof_hdr.dofh_loadsz -
					    userdata->next_chunk;

				if (remaining < 0)
					remaining = 0;

				/*
				 * We've already read a chunk: preserve it and
				 * go on to the next, until we are out.
				 */

				to_copy = MIN(in_bufsz, remaining);
				memcpy(userdata->buf + userdata->next_chunk, in_buf,
				       to_copy);

				userdata->next_chunk += to_copy;
				remaining -= to_copy;

				if (remaining <= 0) {
					userdata->state = DTP_IOCTL_DOF;
					goto chunks_done;
				}

				in.iov_base = (char *) in.iov_base + userdata->next_chunk;
				in.iov_len = MIN(DOF_CHUNKSZ, remaining);
			}
		}

		errmsg = "cannot read DOF";
		if (fuse_reply_ioctl_retry(req, &in, 1, NULL, 0) < 0)
			goto fuse_errmsg;

		if (by_chunks)
			userdata->state = DTP_IOCTL_DOFCHUNK;
		else
			userdata->state = DTP_IOCTL_DOF;
		return;
	}

  chunks_done:
	if (userdata->state != DTP_IOCTL_DOF) {
		errmsg = "FUSE internal state incorrect";
		goto fuse_errmsg;
	}

	/*
	 * Final call: DOF acquired.  Pass to parser for processing, then reply
	 * to unblock the ioctl() caller and return to start state.
	 */

	buf = in_buf;
	if (userdata->buf)
		buf = userdata->buf;

	if (process_dof(req, parser_in_pipe[1], parser_out_pipe[0], pid,
			&userdata->dh, buf) < 0)
		goto process_err;

	if (fuse_reply_ioctl(req, 0, NULL, 0) < 0)
		goto process_err;

	free(userdata->buf);
	userdata->buf = NULL;

	userdata->state = DTP_IOCTL_START;

	return;

  fuse_errmsg:
	fuse_log(FUSE_LOG_ERR, "%i: dtprobed: %s\n", pid, errmsg);

  fuse_err:
	if (fuse_reply_err(req, EINVAL) < 0)
		fuse_log(FUSE_LOG_ERR, "%i: dtprobed: %s\n", pid,
			 "cannot send error to ioctl caller: %s",
			errmsg);
	free(userdata->buf);
	userdata->buf = NULL;
	userdata->state = DTP_IOCTL_START;
	return;

 process_err:
	if (fuse_reply_err(req, EINVAL) < 0)
		fuse_log(FUSE_LOG_ERR, "%i: cannot unblock caller\n",
			 pid);
	free(userdata->buf);
	userdata->buf = NULL;
	userdata->state = DTP_IOCTL_START;
	return;
}

/*
 * Process some DOF, passing it to the parser and creating probes from it.
 */
static int
process_dof(fuse_req_t req, int out, int in, pid_t pid,
	    dof_helper_t *dh, const void *in_buf)
{
	dof_parsed_t *provider;
	const char *errmsg;
	size_t i;

	errmsg = "DOF parser write failed";
	while ((errno = dof_parser_host_write(out, dh,
					      (dof_hdr_t *) in_buf)) == EAGAIN);
	if (errno != 0)
		goto err;

	/*
	 * Wait for parsed reply.
	 */

	errmsg = "parsed DOF read failed";
	provider = dof_read(req, parser_out_pipe[0]);
	if (!provider || provider->type != DIT_PROVIDER)
		goto err;

	for (i = 0; i < provider->provider.nprobes; i++) {
		dof_parsed_t *probe = dof_read(req, in);
		size_t j;

		errmsg = "no probes in this provider, or parse state corrupt";
		if (!probe || probe->type != DIT_PROBE)
			goto err;

		for (j = 0; j < probe->probe.ntp; j++) {
			dof_parsed_t *tp = dof_read(req, in);

			errmsg = "no tracepoints in a probe, or parse state corrupt";
			if (!tp || tp->type != DIT_TRACEPOINT)
				goto err;

			/*
			 * Ignore errors here: we want to create as many probes
			 * as we can, even if creation of some of them fails.
			 */
			create_probe(pid, provider, probe, tp);
			free(tp);
		}
		free(probe);
	}
	free(provider);

	return 0;

err:
	fuse_log(FUSE_LOG_ERR, "%i: dtprobed: parser error: %s\n", pid, errmsg);
	kill(parser_pid, SIGKILL);
	dof_parser_tidy(1);
	return -1;
}	

#if HAVE_LIBFUSE3
static int
loop(void)
{
	struct fuse_buf fbuf = { .mem = NULL };
	struct pollfd fds[1];
	int ret = 0;

	fds[0].fd = fuse_session_fd(cuse_session);
	fds[0].events = POLLIN;

	while (!fuse_session_exited(cuse_session)) {
		if ((ret = poll(fds, 1, -1)) < 0)
			break;

		if (fds[0].revents != 0) {
			if ((ret = fuse_session_receive_buf(cuse_session,
							    &fbuf)) <= 0) {
				if (ret == -EINTR)
					continue;

				break;
			}

			fuse_session_process_buf(cuse_session, &fbuf);
		}
	}

	free(fbuf.mem);
	fuse_session_reset(cuse_session);
	return ret < 0 ? -1 : 0;
}
#else /* libfuse 2 */
static int
loop(void)
{
	struct pollfd fds[1];
	void *buf;
	size_t bufsize;
	int ret = 0;

	bufsize = fuse_chan_bufsize(cuse_chan);
	buf = malloc(bufsize);
	if (!buf) {
		fprintf(stderr, "Cannot allocate memory for FUSE buffer\n");
		return -1;
	}

	fds[0].fd = fuse_chan_fd(cuse_chan);
	fds[0].events = POLLIN;

	while (!fuse_session_exited(cuse_session)) {
		struct fuse_buf fbuf = { .mem = buf, .size = bufsize };
		struct fuse_chan *tmpch = cuse_chan;

		if ((ret = poll(fds, 1, -1)) < 0)
			break;

		if (fds[0].revents != 0) {
			if ((ret = fuse_session_receive_buf(cuse_session,
							    &fbuf, &tmpch)) <= 0) {
				if (ret == -EINTR)
					continue;

				break;
			}

#ifdef HAVE_FUSE_NUMA
			fuse_session_process_buf(cuse_session, &fbuf, tmpch, 0);
#else
			fuse_session_process_buf(cuse_session, &fbuf, tmpch);
#endif
		}
	}

	free(buf);
	fuse_session_reset(cuse_session);
	return ret < 0 ? -1 : 0;
}
#endif

static void
usage(void)
{
	fprintf(stderr, "Syntax: dtprobed [-F] [-d] [-n devname] [-t timeout]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int opt;
	char *devname = "dtrace/helper";
	int sync_fd = -1;
	int ret;
	struct sigaction sa = {0};
	dtprobed_userdata_t userdata = {0};

	/*
	 * These are "command-line" arguments to FUSE itself: our args are
	 * different.  The double-NULL allows us to add an arg.
	 */
	char *fuse_argv[] = { argv[0], "-f", "-s", NULL, NULL };
	int fuse_argc = 3;

	while ((opt = getopt(argc, argv, "Fdn:t:")) != -1) {
		switch (opt) {
		case 'F':
			foreground = 1;
			break;
		case 'n':
			devname = strdup(optarg);
			break;
		case 'd':
			if (!_dtrace_debug) {
				_dtrace_debug = 1;
				fuse_argv[fuse_argc++] = "-d";
			}
			break;
		case 't':
			timeout = atoi(optarg);
			if (timeout <= 0) {
				fprintf(stderr, "Error: timeout must be a "
					"positive integer, not %s\n", optarg);
				exit(1);
			}
			break;
		default:
			usage();
		}
	}

	if (optind < argc)
		usage();

	/*
	 * Close all fds before doing anything else: we cannot close them during
	 * daemonization because CUSE opens fds of its own that we want to keep
	 * around.
	 */
	close_range(3, ~0U, 0);

	if ((cuse_session = setup_helper_device(fuse_argc, fuse_argv,
						devname, &userdata)) == NULL)
		exit(1);

	if (!foreground) {
		if ((sync_fd = daemonize(0)) < 0) {
			teardown_device();
			exit(2);
		}
	}

	/*
	 * We are now daemonized, if we need to be.  Arrange to log to syslog,
	 * fire off the jailed parser subprocess, then report successful startup
	 * down our synchronization pipe (by closing it) and tell systemd (if
	 * present) that we have started.
	 */
	fuse_set_log_func(log_msg);

	/*
	 * Ignore SIGPIPE to allow for a non-hideous way to detect parser
	 * process death.
	 */
	sa.sa_handler = SIG_IGN;
	(void) sigaction(SIGPIPE, &sa, NULL);

	dof_parser_start(sync_fd);

	if (!foreground)
		close(sync_fd);

#ifdef HAVE_LIBSYSTEMD
	sd_notify(1, "READY=1");
#endif

	ret = loop();

	dof_parser_tidy(0);
	teardown_device();

	if (ret == 0)
		exit(0);
	else
		exit(2);			/* Allow restarting.  */
}
