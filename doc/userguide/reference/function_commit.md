
# commit

Commits the speculative buffer, specified by ID, to the principal buffer.

```
void commit(int *id*)
```

The `commit` function is a special function that copies data from a speculative buffer, identified by the provided *id*, into the principal buffer. If more data exists in the specified speculative buffer than the available space in the principal buffer, no data is copied and the drop count for the buffer is incremented.

If the buffer has been speculatively traced on more than one CPU, the speculative data on the committing CPU is copied immediately, while speculative data on other CPUs is copied some time later. Thus, some time might elapse between a commit that begins on one CPU, while the data is copied from speculative buffers to principal buffers on all CPUs. This length of time is guaranteed to be no longer than the time dictated by the cleaning rate.

Further calls to the speculative buffer while a commit is active are handled as follows:

-   `speculation`: the speculative buffer isn't available until each per-CPU speculative buffer has been copied into the corresponding per-CPU principal buffer.
-   `speculate`, `commit`, or `discard`: calls are discarded or fail.

A clause containing a `commit` can't contain a data recording function. However, a clause can contain several `commit` calls to commit disjoint buffers.

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

