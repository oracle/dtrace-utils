/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dt_impl.h>
#include <time.h>

/*
 * These things do not go into the dtrace_hdl both because dt_dprintf doesn't
 * take a dtrace_hdl, and because debugging output from different dtrace_hdls is
 * mixed together when not using the ring buffer, and should appear identical
 * when using it, too.
 */

int _dtrace_debug = 0;		/* debug messages enabled (off) */
int _dtrace_debug_assert = 0;	/* expensive debug assertions enabled (off) */

static char *ring;		/* ring buffer, if non-NULL */
static size_t ring_size;	/* ring size */
static size_t ring_start;	/* start of ring (last printed position, if not
				   wrapped) */
static size_t ring_end;		/* end of ring (last inserted position) */
static int dump_sig = SIGUSR1;	/* ring-dump signal, 0 means 'none' */
static FILE *ring_fd;

_dt_constructor_(_dtrace_debug_init)
static void
_dtrace_debug_init(void)
{
	char *debug_env;
	debug_env = getenv("DTRACE_DEBUG");

	if ((debug_env != NULL) && (debug_env[0] != '\0'))
		_dtrace_debug = 1;

	if (_dtrace_debug && (strcmp(debug_env, "signal") == 0)) {
		char *size_str = getenv("DTRACE_DEBUG_BUF_SIZE");
		char *err;
		size_t size_mb = 128;

		if (size_str) {
			size_mb = strtoul(size_str, &err, 10);
			if (err[0] != '\0') {
				dt_dprintf("Debug buffer size %s invalid.  "
				    "Signal-dump debugging disabled.\n",
				    size_str);
				return;
			}
		}

		ring_size = size_mb * 1024 * 1024;
		ring = malloc(ring_size + 1);

		if (!ring) {
			dt_dprintf("Out of memory allocating debug buffer of "
			    "size %lu MiB.  Signal-dump debugging disabled.\n",
			    size_mb);
		}

		ring[0] = '\0';
		ring_fd = fmemopen(ring, size_mb * 1024 * 1024, "w");
		setvbuf(ring_fd, NULL, _IONBF, 0);
	}

	dtrace_debug_set_dump_sig(dump_sig);
}

void
dtrace_debug_set_dump_sig(int signal)
{
	struct sigaction sa;

	if (!ring)
		return;

	sa.sa_handler = SIG_DFL;
	sigaction(dump_sig, &sa, NULL);

	if (signal == 0) {
		if (ring) {
			fclose(ring_fd);
			free(ring);
			ring = NULL;
		}
	} else {
		sa.sa_handler = dt_debug_dump;
		sa.sa_flags = SA_RESTART;
		sigaction(signal, &sa, NULL);
	}

	dump_sig = signal;
}

/*PRINTFLIKE1*/
_dt_printflike_(1,2)
void
dt_dprintf(const char *format, ...)
{
	va_list alist;

	if (!_dtrace_debug)
		return;

	va_start(alist, format);
	dt_debug_printf("libdtrace", format, alist);
	va_end(alist);
}

void
dt_debug_printf(const char *subsys, const char *format, va_list alist)
{
	if (!_dtrace_debug)
		return;

	if (!ring) {
		flockfile(stderr);
		fprintf(stderr, "%s DEBUG %li: ", subsys, time(NULL));
		vfprintf(stderr, format, alist);
		funlockfile(stderr);
	} else {
		va_list on_err;
		size_t new_ring_end;

		flockfile(ring_fd);
		errno = 0;
		fprintf(ring_fd, "%s DEBUG: %li: ", subsys, time(NULL));
		if (errno == ENOSPC) {
			rewind(ring_fd);
			fprintf(ring_fd, "%s DEBUG: %li: ", subsys, time(NULL));
		}

		va_copy(on_err, alist);

		errno = 0;
		vfprintf(ring_fd, format, alist);
		if (errno == ENOSPC) {
			rewind(ring_fd);
			vfprintf(ring_fd, format, on_err);
		}
		va_end(on_err);

		new_ring_end = ftell(ring_fd);

		/*
		 * Detect wraparound.
		 */
		if ((new_ring_end > ring_start && ring_end < ring_start) ||
		    (new_ring_end > ring_start && new_ring_end < ring_end))
			ring_start = ring_end + 2; /* not +1, as that is '\0' */

		ring_end = new_ring_end;
		funlockfile(ring_fd);
	}
}

void dt_debug_dump(int unused)
{
	if (!ring)
		return;

	if (ring_start < ring_size)
		fprintf(stderr, "%s", &ring[ring_start]);
	if (ring_end < ring_start)
		fprintf(stderr, "%s", ring);

	ring_start = ring_end + 1;
}
