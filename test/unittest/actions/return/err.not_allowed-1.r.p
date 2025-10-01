#!/usr/bin/gawk -f
{
        sub("__.*_sys_getpid", "__*_sys_getpid");
        print;
}
