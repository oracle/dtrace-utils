/*
 * Oracle Linux DTrace.
 * Copyright © 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* A trigger to call open(). */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main (void)
{
	int fd = open ("/dev/null", O_RDONLY);
	return !fd; /* i.e. 0 */
}
