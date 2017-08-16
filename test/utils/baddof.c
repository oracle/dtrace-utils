/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <dtrace.h>

void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", "baddof");
	vfprintf(stderr, fmt, ap);

	if (fmt[strlen(fmt) - 1] != '\n')
		fprintf(stderr, ": %s\n", strerror(errno));

	exit(1);
}

#define	LEAP_DISTANCE		20

void
corrupt(int fd, unsigned char *buf, int len)
{
	static int ttl, valid;
	int bit, i;
	unsigned char saved;
	int val[LEAP_DISTANCE], pos[LEAP_DISTANCE];
	int new, rv;

again:
	printf("valid DOF #%d\n", valid++);

	/*
	 * We are going iterate through, flipping one bit and attempting
	 * to enable.
	 */
	for (bit = 0; bit < len * 8; bit++) {
		saved = buf[bit / 8];
		buf[bit / 8] ^= (1 << (bit % 8));

		if ((bit % 100) == 0)
			printf("%d\n", bit);

		if ((rv = ioctl(fd, DTRACEIOC_ENABLE, buf)) == -1) {
			/*
			 * That failed -- restore the bit and drive on.
			 */
			buf[bit / 8] = saved;
			continue;
		}

		/*
		 * That worked -- and it may have enabled probes.  To keep
		 * enabled probes down to a reasonable level, we'll close
		 * and reopen pseudodevice if we have more than 10,000
		 * probes enabled.
		 */
		ttl += rv;

		if (ttl < 10000) {
			buf[bit / 8] = saved;
			continue;
		}

		printf("enabled %d probes; resetting device.\n", ttl);
		close(fd);

		new = open("/dev/dtrace/dtrace", O_RDWR);

		if (new == -1)
			fatal("couldn't open DTrace pseudo device");

		if (new != fd) {
			dup2(new, fd);
			close(new);
		}

		ttl = 0;
		buf[bit / 8] = saved;
	}

	for (;;) {
		/*
		 * Now we want to get as many bits away as possible.  We flip
		 * bits randomly -- getting as far away as we can until we don't
		 * seem to be making any progress.
		 */
		for (i = 0; i < LEAP_DISTANCE; i++) {
			/*
			 * Pick a random bit and corrupt it.
			 */
			bit = lrand48() % (len * 8);

			val[i] = buf[bit / 8];
			pos[i] = bit / 8;
			buf[bit / 8] ^= (1 << (bit % 8));
		}

		/*
		 * Let's see if that managed to get us valid DOF...
		 */
		if ((rv = ioctl(fd, DTRACEIOC_ENABLE, buf)) > 0) {
			/*
			 * Success!  This will be our new base for valid DOF.
			 */
			ttl += rv;
			goto again;
		}

		/*
		 * No luck -- we'll restore those bits and try flipping a
		 * different set.  Note that this must be done in reverse
		 * order...
		 */
		for (i = LEAP_DISTANCE - 1; i >= 0; i--)
			buf[pos[i]] = val[i];
	}
}

int
main(int argc, char **argv)
{
	char *filename = argv[1];
	dtrace_hdl_t *dtp;
	dtrace_prog_t *pgp;
	int err, fd, len;
	FILE *fp;
	unsigned char *dof, *copy;

	if (argc < 1)
		fatal("expected D script as argument\n");

	if ((fp = fopen(filename, "r")) == NULL)
		fatal("couldn't open %s", filename);

	/*
	 * First, we need to compile our provided D into DOF.
	 */
	if ((dtp = dtrace_open(DTRACE_VERSION, 0, &err)) == NULL) {
		fatal("cannot open dtrace library: %s\n",
		    dtrace_errmsg(NULL, err));
	}

	pgp = dtrace_program_fcompile(dtp, fp, 0, 0, NULL);
	fclose(fp);

	if (pgp == NULL) {
		fatal("failed to compile script %s: %s\n", filename,
		    dtrace_errmsg(dtp, dtrace_errno(dtp)));
	}

	dof = dtrace_dof_create(dtp, pgp, 0);
	len = ((dof_hdr_t *)dof)->dofh_loadsz;

	if ((copy = malloc(len)) == NULL)
		fatal("could not allocate copy of %d bytes", len);

	for (;;) {
		memcpy(copy, dof, len);
		/*
		 * Open another instance of the dtrace device.
		 */
		fd = open("/dev/dtrace/dtrace", O_RDWR);

		if (fd == -1)
			fatal("couldn't open DTrace pseudo device");

		corrupt(fd, copy, len);
		close(fd);
	}

	/* NOTREACHED */
	return (0);
}
