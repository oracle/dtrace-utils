
# normalize

Divides an aggregation value by a specified normalization factor.

```
void normalize(@ *aggr*, uint64_t)
```

The `normalize` function divides an aggregation value by a normalization factor to provide a better view of data within an aggregation. The function takes the aggregation and the normalization factor as arguments. A program used to aggregate data over a period but that presents the data as a per-second occurrence rather than an absolute value is a typical example of a use case for this function.

## How to use normalize to show the number of system calls per second for processes

The normalize function is called against the aggregation. The time is divided to by 1,000,000,000 to convert nanoseconds to seconds.

```
#pragma D option quiet

BEGIN
{
  start = timestamp;
}

syscall:::entry
{
  @func[execname] = count();
}

END
{
  normalize(@func, (timestamp - start) / 1000000000);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

