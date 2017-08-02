/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* Trigger a firing of the dtrace test trigger. */

#define _POSIX_C_SOURCE 200112L

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd, i;

    if ((fd = open("/dev/dtrace/provider/dt_test", O_RDONLY)) == -1) {
        perror("open");
        exit(1);
    }

    for (i = 0; i < 20; i++) {
        int arg = i;
        ioctl(fd, 128, arg);
    }

    close(fd);
}
