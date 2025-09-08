
# ustack

Records a user stack trace to the directed buffer.

```
stack ustack([uint32_t *nframes*, uint32_t *strsize*])
```

The `ustack` function records a user stack trace to the directed buffer. The user stack is, at most, *nframes* in depth. If *nframes* isn't specified, the number of stack frames recorded is the number specified by the `ustackframes` option. While `ustack` can determine the address of the calling frames when the probe fires, the stack frames aren't translated into symbols until the `ustack` function is processed at user level by the DTrace utility. If *strsize* is specified and is non zero, `ustack` allocates the specified amount of string space and then uses it to perform address-to-symbol translation directly from the kernel. Such direct user symbol translation is used only with stacktrace helpers that support this usage with DTrace. If such frames can't be translated, the frames appear only as hexadecimal addresses.

The `ustack` symbol translation occurs after the stack data is recorded. Therefore, the corresponding user process might exit before symbol translation can be performed, making stack frame translation impossible. If the user process exits before symbol translation is performed, `dtrace` outputs a warning message, followed by the hexadecimal stack frames.

## How to use ustack to trace a stack with no address-to-symbol translation

The example shows how to use `ustack` to trace the stack for an `openat` system call by the `date` command.

```
sudo dtrace -qn syscall::openat:entry'/pid == $target/{ustack();}' -c 'date'
```

Generates output similar to the following:

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

