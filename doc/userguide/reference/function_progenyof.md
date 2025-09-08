
# progenyof

Checks whether a calling process is in the progeny of a specified process ID.

```
int progenyof(pid_t)
```

The `progenyof` function returns non zero if the calling process is among the progeny of the specified process ID. The calling process is the process associated with the thread that triggers the matched probe.

## How to use progenyof to limit a clause to list the write system calls for all child processes of a specified process ID

```
syscall::write:entry 
/progenyof($1)/ 
{ 
   @[pid,execname,probefunc]=count()
}
```

This script could be run as follows, to monitor all the system calls that are triggered by a running instance of an application, such as the gnome-terminal-server:

```
sudo dtrace -n 'syscall::write:entry /progenyof($1)/
{@[pid,execname,probefunc]=count()}' $(pidof gnome-terminal-server)
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

