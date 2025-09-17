
# Speculation {#dt_ref_speculation}

DTrace includes a speculative tracing facility that can be used to tentatively trace data at one or more probe locations. You can then decide to commit the data to the principal buffer at another probe location. You can use speculation to trace data that only contains the output that's of interest; no extra processing is required and the DTrace overhead is minimized.

Speculation is achieved by:

-   Setting up a temporary speculation buffer
-   Instructing one or more clauses to trace to the speculation buffer
-   Committing the data in the speculation buffer to the primary buffer; or discarding the speculation buffer.

You can decide to commit or discard speculation data when certain conditions are met, by using the appropriate functions within a clause. By using speculation, you can trace data for a set of probes until a condition is met and then either dispose of the data if it isn't useful, or keep it.

The following table describes DTrace speculation functions.

<table><thead><tr><th>

Function

</th><th>

Args

</th><th>

Description

</th></tr></thead><tbody><tr><td>

`[speculation](../reference/function_speculation.md)`

</td><td>

None

</td><td>

Returns an identifier for a new speculative buffer.

</td></tr><tr><td>

`[speculate](../reference/function_speculate.md)`

</td><td>

ID

</td><td>

Denotes that the remainder of the clause must be traced to the speculative buffer specified by ID.

</td></tr><tr><td>

`[commit](../reference/function_commit.md)`

</td><td>

ID

</td><td>

Commits the speculative buffer that's associated with ID.

</td></tr><tr><td>

`[discard](../reference/function_discard.md)`

</td><td>

ID

</td><td>

Discards the speculative buffer that's associated with ID.

</td></tr><tbody></table>
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

**Parent topic:**[D Program Syntax Reference](../reference/d_program_syntax_reference.md)

