#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>

int str2sig(char *str, int signum)
{
        printf("str2sig %s %d called\n", str, signum);
        return 0;
}

int sig2str(int sig, char *buf)
{
        strcpy(buf, strsignal(sig));
        return 1;
}

