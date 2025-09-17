
# tracemem

Copies the specified number of bytes of data from an address in memory to the current buffer.

```nocopybutton
void tracemem(*addr*, size_t *bytes*[, size_t *limit*])
```

The `tracemem` function copies a specified number of bytes of data, *bytes*, from an address in memory, *addr*, to the current buffer. The address that the data is copied from is specified as a D expression. An optional third argument, *limit*, can be used to limit the size of the data that's copied to the buffer. The limit can be a variable amount, but it must be less than or equal to the size of the memory data that you specified to copy from memory, or it's ignored.

Limiting the data that's copied to the buffer is useful when the data that you're copying has a known upper bound, but the actual number of bytes can vary. DTrace statically reserves *bytes* in the output buffer at compile time. You can reserve a larger amount of memory in the output buffer at run time by setting the number of *bytes*, but dynamically control the amount of memory used by specifying a dynamic *limit*.

## How to use tracemem to trace 256 bytes from an address in memory for the current thread

The example creates a pointer to the current thread by using the built-in variable `curthread`.

```
BEGIN {
     p = curthread;
     tracemem(p, 256);
     exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

