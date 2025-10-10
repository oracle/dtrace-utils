
# stack

Retrieves the kernel call stack.

```
dt_stack_t stack([uint32_t *frames*])
```

Returns a `dt_stack_t` value that can be stored in a variable,
including an associative array element,
or used as a key to an aggregation or associative array.  

When `stack();` appears alone, as a singular action,
it records a stack trace to the output buffer.

One can optionally specify a number of frames.
If no value is specified, the number specified by the `stackframes` runtime option is used.
Frames are included either up to the root frame or until the specified limit has been reached, whichever comes first.

## How to use stack

Here `stack()` is used to assign to a variable and print later using `%k` conversion.

```
        v = stack(3);
        printf("%k", v);
```

In this example, `stack()` is an action that prints the kernel stack.

```
fbt::ksys_write:entry
{
        stack();
        exit(0);
}

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

