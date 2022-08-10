/*
 * Oracle Linux DTrace; close a range of fds.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This is only used if glibc does not provide an implementation, in which case
 * glibc will deal with a kernel too old to implement close_range; but even if
 * it doesn't, if a sufficiently new kernel is in use, we should close all fds
 * using the syscall.
 */

int
close_range(unsigned int first, unsigned int last, unsigned int flags)
{
	DIR *fds;
	struct dirent *ent;

#ifdef __NR_close_range
	int ret;

	ret = syscall(__NR_close_range, first, last, flags);
	if (ret >= 0)
		return ret;
#endif

	fds = opendir("/proc/self/fd");
	if (fds == NULL) {
		/*
		 * No /proc/self/fd: fall back to just closing blindly.
		 */
		struct rlimit nfiles;

		if (getrlimit(RLIMIT_NOFILE, &nfiles) < 0)
			return -1;		/* errno is set for us.  */

		if (nfiles.rlim_max == 0)
			return 0;

		/*
		 * Use rlim_max rather than rlim_cur because one can
		 * lower rlim_cur after opening more than rlim_cur files,
		 * leaving files numbered higher than the limit open.
		 */
		if (last >= nfiles.rlim_max)
			last = nfiles.rlim_max - 1;

		while (first <= last)
			close(first++);

		return 0;
	}

	while (errno = 0, (ent = readdir(fds)) != NULL) {
		char *err;
		int fd;
		fd = strtol(ent->d_name, &err, 10);

		/*
		 * Don't close garbage, no matter what.
		 */
		if (*err != '\0')
			continue;

		if (fd < first || fd > last)
			continue;

		close(fd);
	}
	closedir(fds);
	return 0;
}
