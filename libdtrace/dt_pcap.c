/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <errno.h>
#include <fcntl.h>
#include <pcap.h>
#include <port.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <dt_pcap.h>
#include <dt_impl.h>

typedef struct dt_pcap {
	dt_list_t	dpc_list;
	pthread_mutex_t	dpc_lock;
	dtrace_hdl_t	*dpc_hdl;
	char		*dpc_filename;
	FILE		*dpc_outpipe;
	uint64_t	dpc_linktype;
	uint32_t	dpc_maxlen;
	uint64_t	dpc_boottime;	/* boottime in seconds since epoch */
	pcap_t		*dpc_pcap;
	pcap_dumper_t	*dpc_pcap_dump;
} dt_pcap_t;

dt_pcap_t *
dt_pcap_create(dtrace_hdl_t *dtp, const char *filename, uint32_t maxlen)
{
	dt_pcap_t	*dpc;
	struct sysinfo	info;

	dpc = dt_zalloc(dtp, sizeof (dt_pcap_t));
	if (dpc == NULL) {
		(void) dt_set_errno(dtp, ENOMEM);
		return (NULL);
	}

	dpc->dpc_filename = strdup(filename);
	dpc->dpc_maxlen = maxlen;
	dpc->dpc_outpipe = NULL;
	dpc->dpc_pcap = NULL;
	dpc->dpc_pcap_dump = NULL;
	/*
	 * Times collected in-kernel are relative to boot-time; for wireshark
	 * we need to adjust times relative to the epoch.  We calculate boot
	 * time in seconds since the epoch here; we can then add it to
	 * the capture times we record.
	 */
	if (sysinfo(&info) == 0)
		dpc->dpc_boottime = time(NULL) - info.uptime;
	(void) pthread_mutex_init(&dpc->dpc_lock, NULL);
	dt_list_append(&dtp->dt_pcap.dt_pcaps, dpc);
	return (dpc);
}

void
dt_pcap_destroy(dtrace_hdl_t *dtp)
{
	dt_pcap_t	*dpc, *npc;

	/* flush all captures; happens after tracing is stopped. */
	for (dpc = dt_list_next(&dtp->dt_pcap.dt_pcaps); dpc != NULL; dpc = npc) {
		npc = dt_list_next(dpc);

		dt_list_delete(&dtp->dt_pcap.dt_pcaps, dpc);

		if (dpc->dpc_pcap_dump != NULL) {
			pcap_dump_flush(dpc->dpc_pcap_dump);
			pcap_dump_close(dpc->dpc_pcap_dump);
		}
		if (dpc->dpc_pcap != NULL)
			pcap_close(dpc->dpc_pcap);
		pthread_mutex_destroy(&dpc->dpc_lock);
		free(dpc->dpc_filename);
		dt_free(dtp, dpc);
	}
	if (dtp->dt_pcap.dt_pcap_pid > 0) {
		/*
		 * tshark will now print any remaining output and die.  Wait for
		 * it to do so.
		 */
		close(dtp->dt_pcap.dt_pcap_pipe[1]);
		pthread_join(dtp->dt_pcap.dt_pcap_output, NULL);
		waitpid(dtp->dt_pcap.dt_pcap_pid, NULL, 0);
	}
}

/*
 * The thread that emits output from tshark does nothing but printf()s in a
 * tight loop: stdio ensures that output is nonoverlapping.
 */
static void *
dt_pcap_print(void *d)
{
	dtrace_hdl_t *dtp = d;
	FILE	*inpipe;
	char	*line = NULL;
	size_t	len = 0;

	inpipe = fdopen(dtp->dt_pcap.dt_pcap_pipe[0], "r");
	if (inpipe == NULL) {
		fprintf(stderr, "Cannot open tshark output pipe: %s\n",
		    strerror(errno));
		return (NULL);
	}

	while (getline(&line, &len, inpipe) >= 0) {
		dt_printf(dtp, dtp->dt_pcap.dt_pcap_out_fp,
		    "pcap \t%s", line);
	}
	free(line);
	fclose(inpipe);
	return (NULL);
}

/*
 * The file used to write packet capture data to is either specified via
 * dt_freopen_filename (the result of calling freopen()); or the file is an
 * open fd constituting the input end of a pipe.  If fifo creation or
 * invoking tshark fail, we set the dt_pcap_pid value to -1; this prevents us
 * from retrying these failing operations.  If no file is specified, and a fifo
 * cannot be created for tshark, we return a NULL filename which signifies the
 * caller should fall back to tracemem()-like display of packet data.
 *
 * We indicate to the caller that an open fd is in use by passing back an empty
 * string.  (This is disgusting, but it's simpler than most alternatives, and
 * means that the relevant knowledge can be localized to this file.)
 */
char *
dt_pcap_filename(dtrace_hdl_t *dtp, FILE *fp)
{
	int	status;
	pid_t	pid;
	int	pipe_in[2];
	int	pipe_out[2];

	if (dtp->dt_freopen_filename != NULL &&
	    strcmp(dtp->dt_freopen_filename, DT_FREOPEN_RESTORE) != 0)
		return (strdup(dtp->dt_freopen_filename));

	if (dtp->dt_pcap.dt_pcap_pid < 0) {
		/*
		 * dt_pcap_pid == -1 means we could not create the pipe or run
		 * tshark on a previous attempt; we return NULL which results in
		 * the caller falling back to using tracemem() output of
		 * captured data.
		 */
		return (NULL);
	} else if (dtp->dt_pcap.dt_pcap_pid > 0) {
		/* tshark is running, return the magic we-are-using-a-pipe
		 * value. */
		return ("");
	}

	/*
	 * We have not previously attempted to set up a named pipe and invoke
	 * tshark; let's try it now.
	 */
	dtp->dt_pcap.dt_pcap_pid = -1;

	/*
	 * First, is tshark available? Saves us making assumptions about path.
	 */
	status = system("tshark -v >/dev/null 2>&1");
	if (status < 0 || WEXITSTATUS(status) != 0)
		return (NULL);

	if (pipe(pipe_in) < 0)
		return (NULL);

	if (pipe(pipe_out) < 0) {
		close(pipe_in[0]);
		close(pipe_in[1]);
		return (NULL);
	}

	/*
	 * Finally we invoke tshark to use the pipe as input; the "-i -"
	 * argument will accomplish this.  "-l" forces tshark to flush the pipe
	 * after each packet, rather than allowing data to build up in it.
	 *
	 * We could block and mask SIGCHLD around all this, to avoid trouble if
	 * the process dies, but this is a long-running process, so it will
	 * probably die much later.  Applications intercepting SIGCHLD will
	 * simply have to live with SIGCHLDs from tshark.  (Anyone intercepting
	 * SIGCHLD is probably used to SIGCHLDs from all sorts of things.)
	 */
	pid = fork();
	switch (pid) {
	case 0:
		/*
		 * No need for stderr from tshark.
		 */
		close(2);

		/*
		 * Set up our pipes.
		 */
		close(pipe_in[1]);
		if (pipe_in[0] != STDIN_FILENO) {
			if(dup2(pipe_in[0], STDIN_FILENO) < 0)
				exit(1);
			close(pipe_in[0]);
		}

		close(pipe_out[0]);
		if (pipe_out[1] != STDOUT_FILENO) {
			if(dup2(pipe_out[1], STDOUT_FILENO) < 0)
				exit(1);
			close(pipe_out[1]);
		}

		execlp("tshark", "tshark", "-l", "-i", "-", NULL);
		exit(1);
	case -1:
		/* fork failed; clean up. */
		close(pipe_in[0]);
		close(pipe_in[1]);
		close(pipe_out[0]);
		close(pipe_out[1]);
		break;
	default: {
		/*
		 * In parent; record pid and the appropriate ends of the pipe
		 * fds, and return an empty string to signify that a pipe is in
		 * use.  Kick off a pthread to printf() the output from the
		 * tshark.
		 */
		close(pipe_in[0]);
		close(pipe_out[1]);
		dtp->dt_pcap.dt_pcap_pipe[0] = pipe_out[0];
		dtp->dt_pcap.dt_pcap_pipe[1] = pipe_in[1];
		dtp->dt_pcap.dt_pcap_pid = pid;
		dtp->dt_pcap.dt_pcap_out_fp = fp;

		if (pthread_create(&dtp->dt_pcap.dt_pcap_output, NULL,
			dt_pcap_print, dtp) < 0) {
			/*
			 * No printing thread creatable.  Close the pipes and
			 * give up.  (tshark will terminate immediately.)
			 */
			close(pipe_in[0]);
			close(pipe_in[1]);
			close(pipe_out[0]);
			close(pipe_out[1]);

			waitpid(dtp->dt_pcap.dt_pcap_pid, NULL, 0);
			dtp->dt_pcap.dt_pcap_pid = 0;
			return (NULL);
		}

		return ("");
	}
	}

	return (NULL);
}

void
dt_pcap_dump(dtrace_hdl_t *dtp, char *filename, uint64_t linktype,
	     uint64_t time, void *data, uint32_t datalen, uint32_t maxlen)
{
	struct		pcap_pkthdr hdr;
	dt_pcap_t	*dpc, *npc;
	struct sigaction act, oact;

	for (dpc = dt_list_next(&dtp->dt_pcap.dt_pcaps); dpc != NULL; dpc = npc) {
		npc = dt_list_next(dpc);
		if (strcmp(dpc->dpc_filename, filename) == 0)
			break;
	}

	if (dpc == NULL) {
		dpc = dt_pcap_create(dtp, filename, maxlen);
		if (dpc == NULL)
			return;
	}

	if (dpc->dpc_pcap == NULL) {
		dpc->dpc_pcap = pcap_open_dead((int)linktype,
					       (int)dpc->dpc_maxlen);
		if (dpc->dpc_pcap == NULL) {
			(void) dt_set_errno(dtp, EINVAL);
			return;
		}

		if (dpc->dpc_filename[0] != '\0') {
			dpc->dpc_pcap_dump = pcap_dump_open(dpc->dpc_pcap,
			    dpc->dpc_filename);
		} else {
			int fd;
			fd = fcntl(dtp->dt_pcap.dt_pcap_pipe[1],
			    F_DUPFD_CLOEXEC, 3);

			if (fd >= 0)
				dpc->dpc_outpipe = fdopen(fd, "a");

			if (fd < 0 || dpc->dpc_outpipe == NULL) {
				if (fd >= 0)
					close(fd);
				dt_dprintf("Cannot connect pipe: "
				    "%s\n", strerror(errno));
				(void) dt_set_errno(dtp, errno);
				pcap_close(dpc->dpc_pcap);
				dpc->dpc_pcap = NULL;
				return;
			}

			dpc->dpc_pcap_dump = pcap_dump_fopen(dpc->dpc_pcap,
			    dpc->dpc_outpipe);
		}
		dpc->dpc_linktype = linktype;
	} else if (linktype != dpc->dpc_linktype) {
		/* Handle linktype mismatch here... */
		dt_dprintf("pcap() expected linktype %lu, got %lu.\n",
			   dpc->dpc_linktype, linktype);
		(void) dt_set_errno(dtp, EINVAL);
		return;
	}

	hdr.len = datalen;
	hdr.caplen = datalen;
	if (datalen > maxlen)
		hdr.caplen = maxlen;

	hdr.ts.tv_sec = dpc->dpc_boottime + (time/NANOSEC);
	hdr.ts.tv_usec = (time % NANOSEC) / 1000;

	/*
	 * Reset SIGPIPE here, to avoid SIGPIPEs if tshark dies before we do.
	 */

	bzero(&act, sizeof (act));
	act.sa_handler = SIG_IGN;
	(void) sigaction(SIGPIPE, &act, &oact);

	pthread_mutex_lock(&dpc->dpc_lock);
	pcap_dump((uchar_t *)dpc->dpc_pcap_dump, &hdr, data);

	/*
	 * We flush when using a pipe to avoid a backlog building up in the pipe
	 * buffers.
	 */
	if (dtp->dt_pcap.dt_pcap_pid > 0)
		pcap_dump_flush(dpc->dpc_pcap_dump);
	pthread_mutex_unlock(&dpc->dpc_lock);
	(void) sigaction(SIGCHLD, &oact, NULL);
}
