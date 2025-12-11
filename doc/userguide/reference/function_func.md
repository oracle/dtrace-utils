
# func

Prints the symbol for a specified kernel space address. An alias for `sym`.

```
_symaddr func(uintptr_t *addr*)
```

The `func` function is a data recording function that prints the symbol
that corresponds to a specified kernel space address, *addr*.
The `func` function is an alias for [`sym`](function_sym.md).

## How the func function can return the symbol for a kernel space address

This example uses a bash script to pick a test symbol from `/proc/kallmodsyms` that can be used as a reference in the DTrace program that returns the symbols for the module and function.

```
#!/bin/bash
read ADD <<< $(awk '/ksys_write/ {print $1}' /proc/kallsyms)
dtrace -qn 'BEGIN {func(0x'$ADD'); exit(0)}'
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

