
# discard

Discards a speculative buffer specified by the provided speculation ID.

```
void discard(int *id*)
```

The `discard` function causes DTrace to discard a speculative buffer specified by the provided speculation ID, *id*.

When a speculative buffer is discarded, its contents are also discarded. If the speculation has only been active on the CPU calling `discard`, the buffer is immediately available for further calls to `speculation`. If the speculation has been active on more than one CPU, the discarded buffer is available for further `speculation` some time after the call to `discard`. The length of time between a `discard` on one CPU and the buffer being made available for later speculations is guaranteed to be no longer than the time that's dictated by the cleaning rate. If, at the time `speculation` is called, no buffer is available because all speculative buffers are being discarded or committed, `dtrace` generates a message similar to the following:

```
dtrace: 905 failed speculations (available buffer(s) still busy)
```

You can reduce the likelihood of all buffers being unavailable by tuning the number of speculation buffers or the cleaning rate.

## How to use speculation

The following example illustrates how to use speculation. All speculation functions must be used together for speculation to work correctly.

The speculation is created for the `syscall::open:entry` probe and the ID for the speculation is attached to a thread-local variable. The first argument of the `open()` system call is traced to the speculation buffer by using the `printf` function.

Three more clauses are included for the `syscall::open:return` probe. In the first of these clauses, the `errno` is traced to the speculative buffer. The predicate for the second of the clauses filters for a non-zero `errno` value and commits the speculation buffer. The predicate of the third of the clauses filters for a zero `errno` value and discards the speculation buffer.

The output of the program is returned for the primary data buffer, so the program effectively returns the file name and error number when an `open()` system call fails. If the call doesn't fail, the information that was traced into the speculation buffer is discarded.

```
syscall::open:entry
{
  /*
   * The call to speculation() creates a new speculation. If this fails,
   * dtrace will generate an error message indicating the reason for
   * the failed speculation(), but subsequent speculative tracing will be
   * silently discarded.
   */
  self->spec = speculation();
  speculate(self->spec);

  /*
   * Because this printf() follows the speculate(), it is being
   * speculatively traced; it will only appear in the primary data buffer if the
   * speculation is subsequently committed.
   */
  printf("%s", copyinstr(arg0));
}

syscall::open:return
/self->spec/
{
  /*
   * Trace the errno value into the speculation buffer.
   */
  speculate(self->spec);
  trace(errno);
}

syscall::open:return
/self->spec && errno != 0/
{
  /*
   * If errno is non-zero, commit the speculation.
   */
  commit(self->spec);
  self->spec = 0;
}

syscall::open:return
/self->spec && errno == 0/
{
  /*
   * If errno is not set, discard the speculation.
   */
  discard(self->spec);
  self->spec = 0;
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

