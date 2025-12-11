
# trunc

Truncates an aggregation, meaning discarding aggregation keys.

```nocopybutton
void trunc(@ *aggr*, [int64_t *number*])
```

The `trunc` function takes an aggregation as its first parameter.
While the `clear` function clears only the aggregation's values, retaining the aggregation's keys,
the `trunc` function removes the aggregation's keys.

The optional second argument indicates how many keys to keep.
By default, no keys are kept.

## How to use trunc to show the system call rate only for the most recent ten-second period

The `trunc` function is used inside the `tick-10sec` probe to clear the keys inside the `@func` aggregation.

```
#pragma D option quiet

syscall:::entry
{
  @func[execname] = count();
}

tick-10sec
{
  printa(@func);
  trunc(@func);
}
```

Contrast this behavior with [`clear`](function_clear.md), which
retains keys and simply clears their values.

If the reported aggregations have too many keys, you can use the optional,
second argument to indicate how many keys to retain.
For example, you could limit the reporting to the 5 most common
functions by changing the `tick` probe to:

```
tick-10sec
{
  trunc(@func, 5);
  printa(@func);
  trunc(@func);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)
