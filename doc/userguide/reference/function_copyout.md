
# copyout

Copies the specified size from the specified DTrace buffer to the specified user space address.

```
void copyout(void **src*, uintptr_t *addr*, size_t *size*)
```

The `copyout` function is a destrructive function that copies the specified number of bytes from a specified DTrace buffer to a specified user space address. The user space address is in the address space of the process that associated with the current thread. If the user space address doesn't correspond to a valid, faulted-in page in the current address space, an error is generated.

## How to use copyout to copy data from a DTrace buffer to a specified user space address

The example shows how to use `copyout` to write a string value, `DTrace`, into the user space address for a `write` system call when a user runs the `ls` command. If you run this script, whenever anybody runs the `ls` command on the system, the string `DTrace` replaces the first 5 bytes returned by the command.

```
#pragma D option destructive
syscall::write:entry
/execname == "ls"/
{
    copyout("DTrace", arg1, 5);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

