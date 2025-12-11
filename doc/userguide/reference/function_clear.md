
# clear

Clears the values from an aggregation while retaining aggregation keys.

```nocopybutton
void clear(@ *aggr*)
```

The `clear` function takes an aggregation as its only parameter.
The `clear` function clears only the aggregation's values, while the aggregation's keys are retained.
If the key is referenced after the `clear` function is run, it has a zero value.

## How to use clear to show the system call rate only for the most recent ten-second period

The `clear` function is used inside the `tick-10sec` probe to clear the counter values inside the `@func` aggregation.

```
#pragma D option quiet

BEGIN
{
  last = timestamp;
}

syscall:::entry
{
  @func[execname] = count();
}

tick-10sec
{
  normalize(@func, (timestamp - last) / 1000000000);
  printa(@func);
  clear(@func);
  last = timestamp;
}
```

Each 10 seconds, the counts are cleared.
Since the keys are retained, however, keys will persist in later
time intervals, even if their corresponding functions never reoccur.
More and more keys will be reported with counts of 0.
Thus, you may want to use [`trunc`](function_trunc.md) instead.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)
