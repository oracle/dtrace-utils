
# copyinto

Copies the specified size in bytes from the specified user address into the DTrace scratch buffer and returns the buffer address.

```
void copyinto(uintptr_t *addr*, size_t *size*, void *dest*)
```

The `copyinto` function copies the specified size in bytes, *size*, from the specified user address, *addr*, into the specified DTrace scratch buffer, *dest*. The user address is interpreted as an address in the space of the process that's associated with the current thread. The address in question must correspond to a faulted-in page in the current process. If the address doesn't correspond to a faulted-in page, or if any of the destination memory lies outside of scratch memory, no copying takes place and an error is generated.

## How to use copyinto to copy data from a system write call into an allocated memory buffer

In this example, a probe is set for the entry point on write system calls. A predicate is set to filter for when the process execname matches the `podman` application. The `copyinto` function is used to copy 32 bytes of the first argument, `arg1`, of the write call into a pointer to an allocated memory buffer of 32 bytes, `ptr`. The script prints the a string representation of `ptr` when the `podman` application makes a system write call.

```
syscall::write:entry
/execname=="podman"/
{
        ptr = (char *)alloca(32);
        copyinto(arg1, 32, ptr);
        printf("'%s'", stringof(ptr));
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

