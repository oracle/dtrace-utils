
# ustack

Retrieves the call stack in user space.

```
dt_stack_t ustack([uint32_t *nframes*, uint32_t *strsize*])
```

Returns a `dt_stack_t` value that can be stored in a variable,
including an associative array element, 
or used as a key to an aggregation or associative array.

When `ustack();` appears alone, as a singular action,
it records a user stack trace to the output buffer.

One can optionally specify a number of frames.
If no value is specified, the number specified by the `ustackframes` runtime option is used.
Frames are included either up to the root frame or until the specified limit has been reached, whichever comes first.

Stack frames aren't translated into symbols until the `ustack` function is processed at user level by the DTrace utility.

**Note**:  Historically, if *strsize* was specified and non zero,
`ustack` would allocate the specified amount of string space and then use it to perform address-to-symbol translation directly from the kernel.
Such direct user symbol translation was used only with stacktrace helpers that supported this usage with DTrace.
If such frames could not be translated, the frames would appear only as hexadecimal addresses.
Currently, *strsize* is ignored.

The `ustack` symbol translation occurs after the stack data is recorded.
Therefore, the corresponding user process might exit before symbol translation can be performed, making stack frame translation impossible.
If the user process exits before symbol translation is performed, `dtrace` outputs a warning message, followed by the hexadecimal stack frames.

## How to use ustack

This example shows a D clause that stores the user stack to a global variable,
then later print it with a `%k` conversion:

```
        v = ustack(3);
        printf("%k", v);
```

The example shows how to use `ustack` to trace the stack for an `openat` system call by the `date` command.

```
sudo dtrace -qn syscall::openat:entry'/pid == $target/{ustack();}' -c 'date'
```

This generates output similar to the following:

```
CPU     ID                    FUNCTION:NAME
  2 147861                     openat:entry 
              libc.so.6`__open64_nocancel+0x45
Mon 20 Feb 17:38:15 GMT 2023
              libc.so.6`_nl_find_locale+0xfc
              libc.so.6`setlocale+0x1cf
              date`0x556ebae140ad
              0x7a696c616d726f6e

  2 147861                     openat:entry 
              0x7f6d63fc2e65
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

