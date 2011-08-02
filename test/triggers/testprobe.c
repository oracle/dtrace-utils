#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int fd, i;

    if ((fd = open("/dev/dtrace/provider/dt_test", O_RDONLY)) == -1) {
        perror("open");
        exit(1);
    }

    for (i = 0; i < 20; i++) {
        int arg = i;

        if (ioctl(fd, 128, arg) < 0)
            perror("Req");
    }

    close(fd);
}
