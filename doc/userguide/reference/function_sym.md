
# sym

Prints the symbol for a specified kernel space address. An alias for `func`.

```
_symaddr sym(uintptr_t *addr*)
```

The `sym` function is a data recording function that prints the symbol that corresponds to a specified kernel space address, *addr*.
The `sym` function is an alias for [`func`](function_func.md).

## How the sym function can return the symbol for a kernel space address

This example uses a bash script to pick a test symbol from `/proc/kallmodsyms` that can be used as a reference in the DTrace program that returns the symbol for the function.

```
#!/bin/bash
read ADD <<< `awk '/ksys_write/ {print $1}' /proc/kallmodsyms`
dtrace -qn 'BEGIN {sym(0x'$ADD'); exit(0) }'
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

