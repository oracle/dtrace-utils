
# bcopy

Copies a specified size in bytes from a specified source address outside of scratch memory to a destination address inside scratch memory.

```
void bcopy(void *src*, void *dest*, size_t *size*)
```

The `bcopy` function copies *size* bytes from the memory that's pointed to by *src* to the memory that's pointed to by *dest*. The source memory mustn't be in user space, and the destination memory must be within DTrace scratch memory.

## How to use bcopy to copy data from one memory location to another

In this example, the `bcopy` function is used to copy 14 characters from the ``linux_banner` pointer into a separate memory pointer, `s`, that's allocated 14 bytes of memory. The `printf` line prints a string of the value in stored in the pointer, `s`. The string that's printed is the same as the first 14 characters stored in ``linux_banner`.

```
 BEGIN
 {
         s = (char *)alloca(14);
         bcopy(`linux_banner, &s[0], 13);
         printf("%s\n", stringof(s));
         exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

