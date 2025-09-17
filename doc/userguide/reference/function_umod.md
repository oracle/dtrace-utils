
# umod

Prints the module name that corresponds to a specified user space address.

```
_usymaddr umod(uintptr_t)
```

The `umod` function is a data recording function that prints the name of the module that corresponds to a specified user space address.

## How to use umod to print the module name for an address

The example shows how to use `umod` to print the module names for `openat` system calls by the `date` command.

```
sudo dtrace -qn syscall::openat:entry'/pid == $target/{umod(ucaller);}' -c 'date'
```

Generates output similar to the following:

```
CPU     ID                    FUNCTION:NAME
  7 147861                     openat:entry   libc.so.6                                         
  7 147861                     openat:entry   0x0                                               
Mon 20 Feb 18:07:43 GMT 2023
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

