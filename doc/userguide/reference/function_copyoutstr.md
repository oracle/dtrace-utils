
# copyoutstr

Copies a specified string to a specified user space address.

```
void copyoutstr(char * *string*, uintptr_t *addr*, size_t *size*)
```

The `copyoutstr` function is a destructive function that copies the specified string, *string*, to a specified address, *addr*, in the address space of the process associated with the current thread. A third argument, *size*, is used to control the length of the string. If the user space address doesn't correspond to a valid, faulted-in page in the current address space, an error is generated. Note that the string length is also limited to the value that's set by the compiler and runtime `strsize` option. If *size* exceeds the value `strsize` option, then the string length is limited to the value specified by the `strsize` option.

## How to use copyoutstr to copy a string to a specified user space address

In this example, the `syscall::newuname:entry` and `syscall::newuname:return` probes are used. The entry probe is used to populate a user space address with the first argument used in the entry probe. The return probe writes the string "DTraceHost" into the address of the first argument. When any process makes the `newuname` system call, the hostname part of the call is rewritten.

```
#pragma D option destructive

syscall::newuname:entry 
{ 
  self->a = arg0; 
}

syscall::newuname:return 
{
 copyoutstr("DtraceHost", self->a+65, 128);
}
```

When you run this script and then run the `uname -a` command, output similar to the following is displayed:

```
Linux DtraceHost 5.15.0-7.86.6.1.el8uek.x86_64 #2 SMP ... GNU/Linux
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

