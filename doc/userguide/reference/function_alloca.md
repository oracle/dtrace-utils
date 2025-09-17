
# alloca

Allocates memory and returns a pointer.

```
void alloca(size_t *size*)
```

The `alloca` function allocates *size* bytes out of scratch memory, and returns a pointer to the allocated memory. The returned pointer is guaranteed to have 8–byte alignment. Scratch memory is only valid during the processing of a clause. Memory that's allocated with `alloca` is deallocated when processing of the clause completes. If insufficient scratch memory is available, no memory is allocated and an error is generated.

## How to use alloca to assign a string to an allocated memory region and then to read it out again by using the pointer

```
BEGIN
{
        x = (string *)alloca(sizeof(string) + 1);
        *x = "abc";
        trace(*x);
        exit(0);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

