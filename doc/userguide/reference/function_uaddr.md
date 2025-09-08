
# uaddr

Prints the symbol for a specified address.

```
_usymaddr uaddr(uintptr_t)
```

The `uaddr` function prints the symbol for a specified address, including hexadecimal offset, which enables the same symbol resolution that `ustack` provides.

## How to use uaddr to obtain the symbol for an address

The example shows how to use the `uaddr` function to return the symbols for each `openat` system call when running the `date` command. The clause is predicated to check that the process ID matches the target process. The built-in `ucaller` variable is used to obtain a pointer to the location of the thread at the time the probe is fired.

```
sudo dtrace -n syscall::openat:entry'/pid == $target/{usym(ucaller);}' -c 'date'
```

Generates output similar to the following:

```
CPU     ID                    FUNCTION:NAME
  5 147861                     openat:entry   libc.so.6`_nl_find_locale                         
  5 147861                     openat:entry   0x0                                               
Mon 20 Feb 18:11:30 GMT 2023
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

