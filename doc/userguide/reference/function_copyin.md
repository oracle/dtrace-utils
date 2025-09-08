
# copyin

Copies the specified size from the user address to a DTrace buffer and returns the address of the buffer.

```
void copyin(uintptr_t *addr*, size_t *size*)
```

The `copyin` function copies the specified size in bytes from the specified user address, *addr*, into a DTrace scratch buffer and returns the address of this buffer. The user address is interpreted as an address in the space of the process that's associated with the current thread. The resulting buffer pointer is guaranteed to have 8-byte alignment. The address in question must correspond to a faulted-in page in the current process. If the address doesn't correspond to a faulted-in page, or if insufficient scratch memory is available, NULL is returned, and an error is generated.

## How to use copyin to copy data from a system write call into the DTrace buffer

In this example, a probe is set for the entry point on write system calls. A predicate is set to filter for when the process execname matches the `bash` application. The `copyin` function is used to copy the first argument, `arg1`, and second argument, `arg2`, of the write call to a string which is printed by `printf`. This script prints the argument for the system write calls when somebody uses the `bash` application.

```
syscall::write:entry
/execname=="bash"/
{
    printf("%s", stringof(copyin(arg1,arg2)));
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

