
# exit

Stops all tracing and exits to return an exit value.

```
void exit(int *status*)
```

The `exit` function is used to immediately stop tracing and inform DTrace to do the following: stop tracing, perform any final processing, and call `exit()` with the specified *status* value. Because `exit` returns a status to user level, it's considered a data recording function. However, unlike other data recording functions, `exit` can't be speculatively traced. Note that because `exit` is a data recording function, it can be dropped.

When `exit` is called, only those DTrace functions that are already in progress on other CPUs are completed. No new functions occur on any CPU. The only exception to this rule is the processing of the `END` probe, which is called after the DTrace has processed the `exit` function, and indicates that tracing must stop.

## How to use exit to end all tracing and exit with an exit value

```
BEGIN
{
  trace("hello, world");
  exit(0);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

