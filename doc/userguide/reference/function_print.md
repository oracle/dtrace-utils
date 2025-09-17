
# print

Prints information about a variable.

```
void print(void **addr*)
```

The `print` function is a data recording function that prints information about the variable at the specified address. The function prints the address, type, and data values. This function is aware of kernel and user defined data types, and it recursively descends hierarchies of structures to report on their members. Zero-valued fields are skipped. The dynamic `printsize` option can be used to restrict the amount of data displayed. In the case of a `printsize` overrun, ellipsis \(`...`\) are shown in the output.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

