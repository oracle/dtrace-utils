
# usym

Prints the symbol for a specified address. An alias for `ufunc`.

```
_usymaddr usym(uintptr_t)
```

The `usym` function prints the symbol for a specified address, which is analogous to how [`uaddr`](function_uaddr.md) works,
but without the hexadecimal offsets. The `usym` function is an alias for [`ufunc`](function_ufunc.md).

## How to use usym to obtain the symbol for an address

The example shows how to use the `usym` function to return the symbols for each `openat` system call when running the `date` command. The clause is predicated with a check that the process ID matches the target process. The built-in `ucaller` variable is used to obtain a pointer to the location of the thread at the time the probe is fired.

```
sudo dtrace -n syscall::openat:entry'/pid == $target/{usym(ucaller);}' -c 'date'
```

Generates output similar to the following:

```
CPU     ID                    FUNCTION:NAME
  2 147861                     openat:entry   libc.so.6`_nl_find_locale                         
Mon 20 Feb 18:12:58 GMT 2023
  2 147861                     openat:entry   0x0 
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

