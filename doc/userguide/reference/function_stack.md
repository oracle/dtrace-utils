
# stack

Records a stack trace to the buffer.

```
stack stack([uint32_t *frames*])
```

The `stack` function records a kernel stack trace to the directed buffer. The function includes an option to specify the number of frames deep to record from the kernel stack. If no value is specified, the number of stack frames recorded is the number that's specified by the `stackframes` runtime option. The `dtrace` command reports frames, either up to the root frame or until the specified limit has been reached, whichever comes first.

The `stack` function, having a non-`void` return value, can also be used as the key to an aggregation.

## How to use stack to obtain a kernel stack trace for a particular probe

```
fbt::ksys_write:entry
{
        stack();
        exit(0);
}

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

