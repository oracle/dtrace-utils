/*
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
 * Copyright 2005, 2011, 2015 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* A trigger to call mmap. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define NUMINTS  (1000)
#define FILESIZE (NUMINTS * sizeof(int))

int main(int argc, char *argv[])
{
	int i;
	FILE *foo;
	int fd;
	int *map;

	foo = tmpfile();
	if (foo == NULL) {
		exit(1);
	}
	
	fd = fileno (foo);
	
	if (lseek(fd, FILESIZE-1, SEEK_SET) == -1) {
		exit(1);
	}
	
	if (write(fd, "", 1) == -1) {
		exit(1);
	}
	
	/*
	 * First accomodate the demo/dtrace/begin.d test.
	 */
	for (i = 0; i < 8; i++) {
		map = mmap(0, FILESIZE, i, MAP_SHARED, fd, 0);
		munmap(map, FILESIZE);
	}

	/*
	 * Now perform the mmap for the demo/intro/trussrw.d test.
	 */
	map = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		exit(1);
	}
    
	for (i = 1; i <=NUMINTS; ++i) {
		map[i] = i; 
	}

	munmap(map, FILESIZE);
	close(fd);
	return 0;
}
