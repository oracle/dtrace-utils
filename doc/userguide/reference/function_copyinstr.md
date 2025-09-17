
# copyinstr

Copies a null-terminated C string from the specified user address to a DTrace buffer and returns the address of the buffer.

```
string copyinstr(uintptr_t *addr* [, size_t *size*])
```

The `copyinstr` function copies a null-terminated C string from the specified user address into a DTrace scratch buffer and returns the address of this buffer. The user address is interpreted as an address in the space of the process that's associated with the current thread. An optional maximum length parameter sets a limit on the number of bytes that are examined beyond the address. The resulting string is always null-terminated and the string's length is limited to the value set by the compiler and runtime `strsize` option. As with the `[copyin](function_copyin.md)` function, the specified address must correspond to a faulted-in page in the current process. If the address doesn't correspond to a faulted-in page, or if insufficient scratch memory is available, NULL is returned, and an error is generated.

## How to use copyinstr to copy a string from an address space for a process to the DTrace buffer

In this example, a probe is set for the entry point on write system calls. A predicate is set to filter for when the process execname matches the `passwd` application. The `copyinstr` function is used to copy the first argument, `arg1`, of the write call to a string which is printed by `printf`. This script prints the arguments for the system write calls when somebody uses the `passwd` application to reset a password.

```
syscall::write:entry
/execname=="passwd"/
{
    printf("%s", copyinstr(arg1));
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

