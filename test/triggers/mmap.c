/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
