/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2011 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Read every .d file in the directory in which the test under execution is
 * located.  We assume that this directory does not change while being scanned.
 */

#define _POSIX_C_SOURCE 200112L

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

/* We can in practice rely on PATH_MAX existing: this is pure paranoia. */

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/* This is all rendered excessively complicated by our need to sort the list of
 * dirents to ensure reproducible results (because we must read the files in the
 * same order to keep the sizes passed to read() the same). */

/* The path we are working in. */

const char *path;

/* The directory entry list. */

static struct dirent **dir;
static size_t dir_count = 0;

static int is_regular_test_file (const struct dirent *d);
static void rw_files(const char *path, struct dirent **dir, int outfile);

int
main (void)
{
	char *chop;
	DIR *d;
	FILE *tmp;
	int writefile = -1;

	/* Reduce the testname to a path. */

	path = getenv("_test");
	chop = strrchr(path, '/');
	*chop = '\0';

	if ((d = opendir(path)) == NULL) {
		fprintf(stderr, "%s opening %s\n", strerror(errno), path);
		exit(1);
	}

	/* Read in the contents of the directory, sorted. */

	dir_count = scandir(path, &dir, is_regular_test_file, alphasort);
	if (dir_count < 0) {
		fprintf(stderr, "%s reading directories from %s\n",
		    strerror(errno), path);
		exit(1);
	}

	closedir(d);

	/*
	 * read() as much as we can from each file, and write it out again in
	 * identically-sized chunks to a temporary file.
	 */
	if ((tmp = tmpfile()) != NULL) {
		writefile = fileno(tmp);
	}
	if (writefile < 0) {
		perror("creating temporary write-test file");
		exit(1);
	}

	rw_files(path, dir, writefile);
	fclose(tmp);

	return 0;
}

/* Identify regular .d files. */

static int
is_regular_test_file (const struct dirent *d)
{
	struct stat s;
	char name[PATH_MAX];
	size_t namelen = strlen (d->d_name);

	/* No .d suffix. */

	if ((namelen < 2) ||
	    (d->d_name[namelen-2] != '.') ||
	    (d->d_name[namelen-1] != 'd'))
		return 0;

	/*
	 * Check filetype.  Don't use d_type at all, to prevent inconsistent
	 * results depending on the fs on which the test is run.
	 */
	strcpy(name, path);
	strcat(name, "/");
	strcat(name, d->d_name);
	
	if (lstat(name, &s) < 0) {
		/* error -> not a regular file, whatever it is */
		return 0;
	}
	if (!S_ISREG(s.st_mode))
		return 0;

	return 1;
}

/*
 * Read the contents of the files named in DIR, located under PATH, writing them
 * out in identically-sized chunks to the OUTFILE.  Any read/write/open errors
 * are immediately fatal.  Short writes are not diagnosed and will lead to a
 * test failure.
 */
static void
rw_files(const char *path, struct dirent **dir, int outfile)
{
	size_t i;
	struct dirent *d;

	for (i = 0; i < dir_count; i++) {
		char name[PATH_MAX];
		char buf[4096];
		int infile;
		ssize_t nbytes;

		d = dir[i];

		strcpy(name, path);
		strcat(name, "/");
		strcat(name, d->d_name);

		if ((infile = open(name, O_RDONLY)) < 0) {
			fprintf(stderr, "Cannot open %s: %s\n", name,
			    strerror(errno));
			exit(1);
		}

		while((nbytes = read(infile, buf, 4096)) > 0) {
			if (write(outfile, buf, nbytes) < 0) {
				fprintf(stderr, "Cannot write %li bytes: %s\n",
				    nbytes, strerror(errno));
				exit(1);
			}
		}
		
		if(nbytes < 0) {
			fprintf (stderr, "Cannot read from %s: %s\n",
			    name, strerror(errno));
			exit(1);
		}
		close(infile);
	}
}
