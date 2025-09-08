
# mod

Prints the module name that corresponds to a specified kernel space address.

```
_symaddr mod(uintptr_t *addr*)
```

The `mod` function is a data recording function that prints the name of the module that corresponds to a specified kernel space address.

## How to use mod to print the module name for a pointer to a specified kernel space address

This example uses a bash script to pick a test symbol from `/proc/kallmodsyms` that can be used as a reference in the DTrace program that returns the symbol for the module. Note that where a module is effectively empty in `/proc/kallmodsyms` it's the same as a value of `vmlinux`.

```
#!/bin/bash
read ADD <<< `awk '/ksys_write/ {print $1}' /proc/kallmodsyms`
dtrace -qn 'BEGIN {mod(0x'$ADD'); exit(0) }'
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

