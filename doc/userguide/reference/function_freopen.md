
# freopen

Changes the file associated with `stdout` to a specified file.

```
void freopen(const char *pathname*, ...)
```

The `freopen` function is typically a data recording function that changes the file that's associated with `stdout` to the file that's specified by the arguments in `printf` fashion.

If the `""` string is used, the output is again restored to `stdout`.

The `freopen` function isn't only data-recording but also destructive, because you can use it to overwrite arbitrary files.

## How to use freopen to write to a specified file and log a system call

The script opens with a pragma to enable destructive functions in DTrace. You can alternatively remove this line and run the script with `dtrace -w`. The `freopen` function is destructive because it writes to a file on the file system and can overwrite existing files. The example creates a temporary log file to track the process names that make a `mkdir` system call while the program is running.

```
 #pragma D option destructive
 dtrace:::BEGIN
 {
        freopen("/tmp/dlog");
 
 }
 syscall:vmlinux:mkdir:entry
 {
        printf("%Y-> %s \n",walltimestamp,execname);
 }

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

